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
        layout = layout->addName(s.left()->id());
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
            block->append(new InstrConstNone);
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
        block->append(new InstrGetMethod(name));
        block->append(new InstrCall(1));
    }

    void callBinaryMethod(const BinarySyntax& s, string name) {
        s.left()->accept(*this);
        block->append(new InstrGetMethod(name));
        s.right()->accept(*this);
        block->append(new InstrCall(2));
    }

    virtual void visit(const SyntaxBlock& s) {
        for (auto i = s.stmts().begin(); i != s.stmts().end(); ++i)
            (*i)->accept(*this);
    }

    virtual void visit(const SyntaxInteger& s) {
        block->append(new InstrConstInteger(s.value()));
    }

    virtual void visit(const SyntaxName& s) {
        block->append(new InstrGetLocal(s.id()));
    }

    virtual void visit(const SyntaxOr& s) {
        s.left()->accept(*this);
        InstrOrBranch* branch = new InstrOrBranch;
        block->append(branch);
        s.right()->accept(*this);
        branch->setDest(block->nextInstr());
    }

    virtual void visit(const SyntaxAnd& s) {
        s.left()->accept(*this);
        InstrAndBranch* branch = new InstrAndBranch;
        block->append(branch);
        s.right()->accept(*this);
        branch->setDest(block->nextInstr());
    }

    virtual void visit(const SyntaxNot& s) {
        s.right()->accept(*this);
        block->append(new InstrNot);
    }

    virtual void visit(const SyntaxPos& s) { callUnaryMethod(s, "__pos__"); }
    virtual void visit(const SyntaxNeg& s) { callUnaryMethod(s, "__neg__"); }
    virtual void visit(const SyntaxInvert& s) { callUnaryMethod(s, "__invert__"); }

#define define_vist_binary_as_method_call(syntax, method)                     \
    virtual void visit(const syntax& s) { callBinaryMethod(s, method); }

    define_vist_binary_as_method_call(SyntaxLT, "__lt__");
    define_vist_binary_as_method_call(SyntaxLE, "__le__");
    define_vist_binary_as_method_call(SyntaxGT, "__gt__");
    define_vist_binary_as_method_call(SyntaxGE, "__ge__");
    define_vist_binary_as_method_call(SyntaxEQ, "__eq__");
    define_vist_binary_as_method_call(SyntaxNE, "__ne__");
    define_vist_binary_as_method_call(SyntaxBitOr, "__or__");
    define_vist_binary_as_method_call(SyntaxBitXor, "__xor__");
    define_vist_binary_as_method_call(SyntaxBitAnd, "__and__");
    define_vist_binary_as_method_call(SyntaxBitLeftShift, "__lshift__");
    define_vist_binary_as_method_call(SyntaxBitRightShift, "__rshift__");
    define_vist_binary_as_method_call(SyntaxPlus, "__add__");
    define_vist_binary_as_method_call(SyntaxMinus, "__sub__");
    define_vist_binary_as_method_call(SyntaxMultiply, "__mul__");
    define_vist_binary_as_method_call(SyntaxDivide, "__div__");
    define_vist_binary_as_method_call(SyntaxIntDivide, "__floordiv__");
    define_vist_binary_as_method_call(SyntaxModulo, "__mod__");
    define_vist_binary_as_method_call(SyntaxPower, "__pow__");

#undef define_vist_binary_as_method_call

#define define_vist_binary_instr(syntax, instr)                               \
    virtual void visit(const syntax& s) {                                     \
        s.left()->accept(*this);                                              \
        s.right()->accept(*this);                                             \
        block->append(new instr);                                             \
    }

    define_vist_binary_instr(SyntaxIn, InstrIn);
    define_vist_binary_instr(SyntaxIs, InstrIs);

#undef define_vist_binary_instr

    virtual void visit(const SyntaxPropRef& s) {
        s.left()->accept(*this);
        block->append(new InstrGetProp(s.right()->id()));
    }

    virtual void visit(const SyntaxAssignName& s) {
        s.right()->accept(*this);
        block->append(new InstrSetLocal(s.left()->id()));
    }

    virtual void visit(const SyntaxAssignProp& s) {
        s.left()->left()->accept(*this);
        s.right()->accept(*this);
        block->append(new InstrSetProp(s.left()->right()->id()));
    }

    virtual void visit(const SyntaxCall& s) {
        // todo: check this actually how python works
        bool methodCall = s.left()->is<SyntaxPropRef>();

        if (methodCall) {
            const SyntaxPropRef* pr = s.left()->as<SyntaxPropRef>();
            pr->left()->accept(*this);
            block->append(new InstrGetMethod(pr->right()->as<SyntaxName>()->id()));
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
  : layout(Frame::ObjectClass->layout())
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

void testBuildRaw(const string& input, const string& expected)
{
    BlockBuilder bb;
    bb.buildRaw(input);
    Block* block = bb.takeBlock();
    testEqual(repr(block), expected);
    delete block;
}

void testBuild(const string& input, const string& expected)
{
    BlockBuilder bb;
    bb.build(input);
    Block* block = bb.takeBlock();
    testEqual(repr(block), expected);
    delete block;
}

testcase(block)
{
    testBuildRaw("3", "ConstInteger 3");
    testBuildRaw("foo.bar", "GetLocal foo, GetProp bar");
    testBuildRaw("foo()", "GetLocal foo, Call 0");
    testBuildRaw("foo(bar, baz)", "GetLocal foo, GetLocal bar, GetLocal baz, Call 2");
    testBuildRaw("foo.bar(baz)", "GetLocal foo, GetMethod bar, GetLocal baz, Call 2");
    testBuildRaw("foo = 1", "ConstInteger 1, SetLocal foo");
    testBuildRaw("foo.bar = baz", "GetLocal foo, GetLocal baz, SetProp bar");
    testBuildRaw("foo + 1",
                 "GetLocal foo, GetMethod __add__, ConstInteger 1, Call 2");
    testBuild("1", "ConstInteger 1, ConstNone, Return");
    testBuild("return 1", "ConstInteger 1, Return");
    testBuild("return foo in bar", "GetLocal foo, GetLocal bar, In, Return");
    testBuild("return foo is not bar", "GetLocal foo, GetLocal bar, Is, Not, Return");
    testBuild("return 2 - - 1",
              "ConstInteger 2, GetMethod __sub__, ConstInteger 1, GetMethod __neg__, Call 1, Call 2, Return");
}
