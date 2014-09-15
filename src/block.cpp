#include "block.h"
#include "syntax.h"
#include "instr.h"
#include "test.h"
#include "utility.h"

struct BlockBuilder : public SyntaxVisitor
{
    BlockBuilder() : block(nullptr) {}
    ~BlockBuilder() { delete block; }

    void start(const Input& input) {
        parser.start(input);
    }

    void build() {
        assert(!block);
        block = new Block;
        while (!parser.atEnd())
            parser.parse()->accept(*this);
    }

    void buildTopLevel() {
        assert(!block);
        block = new Block;
        block->append(new InstrConstNumber(0)); // todo: should be None
        while (!parser.atEnd())
            parser.parse()->accept(*this);
        block->append(new InstrReturn());
    }

    Block* takeBlock() {
        Block* result = block;
        block = nullptr;
        return result;
    }

  private:
    SyntaxParser parser;
    Block *block;

    void callUnaryMethod(const UnarySyntax& s, std::string name) {
        s.right->accept(*this);
        block->append(new InstrDup());
        block->append(new InstrGetProp(name));
        block->append(new InstrSwap());
        block->append(new InstrCall(1));
    }

    void callBinaryMethod(const BinarySyntax& s, std::string name) {
        s.left->accept(*this);
        block->append(new InstrDup());
        block->append(new InstrGetProp(name));
        block->append(new InstrSwap());
        s.right->accept(*this);
        block->append(new InstrCall(2));
    }

    virtual void visit(const SyntaxNumber& s) {
        block->append(new InstrConstNumber(s.value));
    }

    virtual void visit(const SyntaxName& s) {
        block->append(new InstrGetLocal(s.id));
    }

    // todo: actual names
    virtual void visit(const SyntaxNegate& s) { callUnaryMethod(s, "__negate__"); }
    virtual void visit(const SyntaxPlus& s) { callBinaryMethod(s, "__plus__"); }
    virtual void visit(const SyntaxMinus& s) { callBinaryMethod(s, "__minus__"); }

    virtual void visit(const SyntaxPropRef& s) {
        s.left->accept(*this);
        block->append(new InstrGetProp(s.right->as<SyntaxName>()->id));
    }

    virtual void visit(const SyntaxAssign& s) {
        assert(s.left->is<SyntaxName>() || s.left->is<SyntaxPropRef>());
        if (s.left->is<SyntaxName>()) {
            SyntaxName* n = s.left->as<SyntaxName>();
            s.right->accept(*this);
            block->append(new InstrSetLocal(n->id));
        } else {
            SyntaxPropRef* pr = s.left->as<SyntaxPropRef>();
            pr->left->accept(*this);
            s.right->accept(*this);
            block->append(new InstrSetProp(pr->name()));
        }
    }

    virtual void visit(const SyntaxCall& s) {
        // todo: check this actually how python works
        bool methodCall = s.left->is<SyntaxPropRef>();

        if (methodCall) {
            SyntaxPropRef* pr = s.left->as<SyntaxPropRef>();
            pr->left->accept(*this);
            block->append(new InstrDup());
            block->append(new InstrGetProp(pr->right->as<SyntaxName>()->id));
            block->append(new InstrSwap());
        } else {
            s.left->accept(*this);
        }

        for (auto i = s.right.begin(); i != s.right.end(); ++i)
            (*i)->accept(*this);
        unsigned count = s.right.size() + (methodCall ? 1 : 0);
        block->append(new InstrCall(count));
    }
};


Block* Block::buildTopLevel(const Input& input)
{
    BlockBuilder builder;
    builder.start(input);
    builder.buildTopLevel();
    return builder.takeBlock();
}

Block::~Block()
{
    for (unsigned i = 0; i < instrs.size(); ++i)
        delete instrs[i];
}

std::ostream& operator<<(std::ostream& s, Block* block) {
    for (unsigned i = 0; i < block->instrCount(); ++i) {
        if (i > 0)
            s << ", ";
        s << block->instr(i);
    }
    return s;
}

testcase("block", {
    BlockBuilder bb;

    bb.start("3");
    bb.build();
    Block* block = bb.takeBlock();
    testEqual(repr(block), "ConstNumber 3");
    delete block;

    bb.start("foo.bar");
    bb.build();
    block = bb.takeBlock();
    testEqual(repr(block), "GetLocal foo, GetProp bar");
    delete block;

    bb.start("foo()");
    bb.build();
    block = bb.takeBlock();
    testEqual(repr(block), "GetLocal foo, Call 0");
    delete block;

    bb.start("foo(bar, baz)");
    bb.build();
    block = bb.takeBlock();
    testEqual(repr(block), "GetLocal foo, GetLocal bar, GetLocal baz, Call 2");
    delete block;

    bb.start("foo.bar(baz)");
    bb.build();
    block = bb.takeBlock();
    testEqual(repr(block), "GetLocal foo, Dup, GetProp bar, Swap, GetLocal baz, Call 2");
    delete block;

    bb.start("foo = 1");
    bb.build();
    block = bb.takeBlock();
    testEqual(repr(block), "ConstNumber 1, SetLocal foo");
    delete block;

    bb.start("foo.bar = baz");
    bb.build();
    block = bb.takeBlock();
    testEqual(repr(block), "GetLocal foo, GetLocal baz, SetProp bar");
    delete block;

    bb.start("foo + 1");
    bb.build();
    block = bb.takeBlock();
    testEqual(repr(block),
              "GetLocal foo, Dup, GetProp __plus__, Swap, ConstNumber 1, Call 2");
    delete block;

});
