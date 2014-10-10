#include "block.h"
#include "syntax.h"
#include "instr.h"
#include "test.h"
#include "repr.h"

#include <memory>

//#define TRACE_BUILD

unsigned Block::append(Instr* instr)
{
    assert(instr);
    instrs.push_back(instr);
    return instrs.size() - 1;
}

void Block::branchHere(unsigned source)
{
    assert(source < instrs.size());
    Branch* b = instrs[source]->asBranch();
    b->setOffset(instrs.size() - source);
}

// Find the names that are defined in the current block
struct DefinitionFinder : public DefaultSyntaxVisitor
{
    Layout* layout;

    DefinitionFinder() : layout(nullptr) {}

    static Layout* buildLayout(Syntax *s) {
        DefinitionFinder df;
        s->accept(df);
        return df.layout;
    }

    virtual void visit(const SyntaxAssignName& s) {
        layout = layout->addName(s.left()->id());
    }

    // Recurse into blocks etc.

    virtual void visit(const SyntaxCond& s) {
        s.cons()->accept(*this);
        s.alt()->accept(*this);
    }
};

struct BlockBuilder : public SyntaxVisitor
{
    BlockBuilder() : block(nullptr) {}
    ~BlockBuilder() { delete block; }

    void build(Syntax* s) {
        assert(!block);
        block = new Block;
        layout = DefinitionFinder::buildLayout(s);  // todo: use this!
        s->accept(*this);
    }

    void buildRaw(const Input& input) {
        SyntaxParser parser;
        parser.start(input);
        unique_ptr<Syntax> syntax(parser.parseBlock());
        build(syntax.get());
    }

    void buildFunctionBody(const Input& input) {
        buildRaw(input);
        if (block->instrCount() == 0 || !block->lastInstr()->is<InstrReturn>()) {
            block->append(new InstrConstNone);
            block->append(new InstrReturn());
        }
#ifdef TRACE_BUILD
        cerr << repr(block) << endl;
#endif
    }

    Block* takeBlock() {
        Block* result = block;
        block = nullptr;
        return result;
    }

  private:
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
        unsigned branch = block->append(new InstrBranchIfTrue);
        s.right()->accept(*this);
        block->branchHere(branch);
    }

    virtual void visit(const SyntaxAnd& s) {
        s.left()->accept(*this);
        unsigned branch = block->append(new InstrBranchIfFalse);
        s.right()->accept(*this);
        block->branchHere(branch);
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

    virtual void visit(const SyntaxCond& s) {
        s.cond()->accept(*this);
        unsigned altBranch = block->append(new InstrBranchIfFalse);
        s.cons()->accept(*this);
        unsigned endBranch = block->append(new InstrBranchAlways);
        block->branchHere(altBranch);
        s.alt()->accept(*this);
        block->branchHere(endBranch);
    }

    virtual void visit(const SyntaxLambda& a) {
        BlockBuilder exprBuilder;
        exprBuilder.build(a.expr());
        Block* exprBlock = exprBuilder.takeBlock();
        exprBlock->append(new InstrReturn);
        block->append(new InstrLambda(a.params(), exprBlock));
    }
};


Block* Block::buildTopLevel(const Input& input)
{
    BlockBuilder builder;
    builder.buildFunctionBody(input);
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
    Instr** ptr = block->startInstr();
    for (unsigned i = 0; i < block->instrCount(); ++i, ++ptr) {
        if (i > 0)
            s << ", ";
        (*ptr)->print(s, ptr);
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
    bb.buildFunctionBody(input);
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
