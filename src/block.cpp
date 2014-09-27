#include "block.h"
#include "syntax.h"
#include "instr.h"
#include "test.h"
#include "repr.h"

#include <memory>

// Find the names that are defined in the current block
struct DefinitionFinder : public DefaultSyntaxVisitor
{
    Layout* layout;

    DefinitionFinder() : layout(nullptr) {}

    virtual void visit(const SyntaxAssignName& s) {
        layout = layout->addName(s.left()->id);
    }

    // todo: recurse into blocks etc

    static Layout* buildLayout(Syntax *s) {
        DefinitionFinder df;
        s->accept(df);
        return df.layout;
    }
};

struct BlockBuilder : public SyntaxVisitor
{
    BlockBuilder() : block(nullptr) {}
    ~BlockBuilder() { delete block; }

    void buildRaw(const Input& input) {
        parser.start(input);
        assert(!block);
        block = new Block;
        unique_ptr<Syntax> syntax(parser.parseBlock());
        layout = DefinitionFinder::buildLayout(syntax.get());
        syntax->accept(*this);
    }

    void build(const Input& input) {
        buildRaw(input);
        if (block->instrCount() == 0 || !block->lastInstr()->is<InstrReturn>()) {
            block->append(new InstrConstInteger(0)); // todo: should be None
            block->append(new InstrReturn());
        }
    }

    Block* takeBlock() {
        Block* result = block;
        block = nullptr;
        return result;
    }

  private:
    SyntaxParser parser;
    Layout* layout;
    Block* block;

    void callUnaryMethod(const UnarySyntax& s, string name) {
        s.right()->accept(*this);
        block->append(new InstrDup());
        block->append(new InstrGetProp(name));
        block->append(new InstrSwap());
        block->append(new InstrCall(1));
    }

    void callBinaryMethod(const BinarySyntax& s, string name) {
        s.left()->accept(*this);
        block->append(new InstrDup());
        block->append(new InstrGetProp(name));
        block->append(new InstrSwap());
        s.right()->accept(*this);
        block->append(new InstrCall(2));
    }

    virtual void visit(const SyntaxBlock& s) {
        for (auto i = s.stmts().begin(); i != s.stmts().end(); ++i)
            (*i)->accept(*this);
    }

    virtual void visit(const SyntaxInteger& s) {
        block->append(new InstrConstInteger(s.value));
    }

    virtual void visit(const SyntaxName& s) {
        block->append(new InstrGetLocal(s.id));
    }

    // todo: actual names
    virtual void visit(const SyntaxNegate& s) { callUnaryMethod(s, "__negate__"); }
    virtual void visit(const SyntaxPlus& s) { callBinaryMethod(s, "__plus__"); }
    virtual void visit(const SyntaxMinus& s) { callBinaryMethod(s, "__minus__"); }

    virtual void visit(const SyntaxPropRef& s) {
        s.left()->accept(*this);
        block->append(new InstrGetProp(s.right()->id));
    }

    virtual void visit(const SyntaxAssignName& s) {
        s.right()->accept(*this);
        block->append(new InstrSetLocal(s.left()->id));
    }

    virtual void visit(const SyntaxAssignProp& s) {
        s.left()->left()->accept(*this);
        s.right()->accept(*this);
        block->append(new InstrSetProp(s.left()->right()->id));
    }

    virtual void visit(const SyntaxCall& s) {
        // todo: check this actually how python works
        bool methodCall = s.left()->is<SyntaxPropRef>();

        if (methodCall) {
            const SyntaxPropRef* pr = s.left()->as<SyntaxPropRef>();
            pr->left()->accept(*this);
            // todo: replace this dup / getprop / swap sequence with a
            // prepMethod instruction
            block->append(new InstrDup());
            block->append(new InstrGetProp(pr->right()->as<SyntaxName>()->id));
            block->append(new InstrSwap());
        } else {
            s.left()->accept(*this);
        }

        for (auto i = s.right().begin(); i != s.right().end(); ++i)
            (*i)->accept(*this);
        unsigned count = s.right().size() + (methodCall ? 1 : 0);
        block->append(new InstrCall(count));
    }

    virtual void visit(const SyntaxReturn& s) {
        s.right()->accept(*this);
        block->append(new InstrReturn);
    }
};


Block* Block::buildTopLevel(const Input& input)
{
    BlockBuilder builder;
    builder.build(input);
    return builder.takeBlock();
}

Block::Block()
  : layout(Frame::Class.getLayout())
{}

Block::~Block()
{
    for (unsigned i = 0; i < instrs.size(); ++i)
        delete instrs[i];
}

ostream& operator<<(ostream& s, Block* block) {
    for (unsigned i = 0; i < block->instrCount(); ++i) {
        if (i > 0)
            s << ", ";
        s << block->instr(i);
    }
    return s;
}

testcase(block)
{
    BlockBuilder bb;

    bb.buildRaw("3");
    Block* block = bb.takeBlock();
    testEqual(repr(block), "ConstInteger 3");
    delete block;

    bb.buildRaw("foo.bar");
    block = bb.takeBlock();
    testEqual(repr(block), "GetLocal foo, GetProp bar");
    delete block;

    bb.buildRaw("foo()");
    block = bb.takeBlock();
    testEqual(repr(block), "GetLocal foo, Call 0");
    delete block;

    bb.buildRaw("foo(bar, baz)");
    block = bb.takeBlock();
    testEqual(repr(block), "GetLocal foo, GetLocal bar, GetLocal baz, Call 2");
    delete block;

    bb.buildRaw("foo.bar(baz)");
    block = bb.takeBlock();
    testEqual(repr(block), "GetLocal foo, Dup, GetProp bar, Swap, GetLocal baz, Call 2");
    delete block;

    bb.buildRaw("foo = 1");
    block = bb.takeBlock();
    testEqual(repr(block), "ConstInteger 1, SetLocal foo");
    delete block;

    bb.buildRaw("foo.bar = baz");
    block = bb.takeBlock();
    testEqual(repr(block), "GetLocal foo, GetLocal baz, SetProp bar");
    delete block;

    bb.buildRaw("foo + 1");
    block = bb.takeBlock();
    testEqual(repr(block),
              "GetLocal foo, Dup, GetProp __plus__, Swap, ConstInteger 1, Call 2");
    delete block;

    bb.build("1");
    block = bb.takeBlock();
    testEqual(repr(block), "ConstInteger 1, ConstInteger 0, Return");
    delete block;

    bb.build("return 1");
    block = bb.takeBlock();
    testEqual(repr(block), "ConstInteger 1, Return");
    delete block;
}
