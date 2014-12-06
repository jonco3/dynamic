#include "block.h"
#include "builtin.h"
#include "instr.h"
#include "repr.h"
#include "string.h"
#include "syntax.h"

#include <memory>

//#define TRACE_BUILD

Block::Block(Traced<Layout*> layout)
  : layout_(layout)
{}

unsigned Block::append(Instr* instr)
{
    assert(instr);
    unsigned index = nextIndex();
    instrs_.push_back(instr);
    return index;
}

void Block::branchHere(unsigned source)
{
    assert(source < instrs_.size() - 1);
    Branch* b = instrs_[source]->asBranch();
    b->setOffset(instrs_.size() - source);
}

int Block::offsetTo(unsigned dest)
{
    assert(dest < instrs_.size() - 1);
    return dest - instrs_.size();
}

void Block::print(ostream& s) const {
    for (auto i = instrs_.begin(); i != instrs_.end(); ++i) {
        if (i != instrs_.begin())
            s << ", ";
        (*i)->print(s);
    }
}

void Block::traceChildren(Tracer& t)
{
    gc::trace(t, &layout_);
    for (auto i = instrs_.begin(); i != instrs_.end(); ++i)
        gc::trace(t, &*i);
}

// Find the names that are defined in the current block
struct DefinitionFinder : public DefaultSyntaxVisitor
{
    DefinitionFinder(Layout* layout) : layout_(layout) {}

    static Layout* buildLayout(Syntax *s, Layout* layout) {
        DefinitionFinder df(layout);
        s->accept(df);
        return df.layout_;
    }

    void addName(Name name) {
        layout_ = layout_->maybeAddName(name);
    }

    // Record assignments and definintions

    virtual void visit(const SyntaxAssignName& s) {
        addName(s.left()->id());
    }

    virtual void visit(const SyntaxDef& s) {
        addName(s.id());
    }

    // Recurse into local blocks

    virtual void visit(const SyntaxBlock& s) {
        for (auto i = s.stmts().begin(); i != s.stmts().end(); ++i)
            (*i)->accept(*this);
    }

    virtual void visit(const SyntaxCond& s) {
        s.cons()->accept(*this);
        s.alt()->accept(*this);
    }

    virtual void visit(const SyntaxIf& s) {
        for (auto i = s.branches().begin(); i != s.branches().end(); ++i)
            (*i).block->accept(*this);
        if (s.elseBranch())
            s.elseBranch()->accept(*this);
    }

    virtual void visit(const SyntaxWhile& s) {
        s.suite()->accept(*this);
        if (s.elseSuite())
            s.elseSuite()->accept(*this);
    }

  private:
    Root<Layout*> layout_;
};

struct BlockBuilder : public SyntaxVisitor
{
    BlockBuilder(BlockBuilder* parent = nullptr)
      : parent(parent),
        topLevel(nullptr),
        layout(Object::InitialLayout),
        block(nullptr) {}

    void setParams(const vector<Name>& params) {
        assert(parent);
        assert(layout == Object::InitialLayout);
        assert(!topLevel);
        for (auto i = params.begin(); i != params.end(); ++i)
            layout = layout->addName(*i);
    }

    void setGlobals(Traced<Object*> globals) {
        assert(globals);
        assert(!parent);
        assert(layout == Object::InitialLayout);
        assert(!topLevel);
        topLevel = globals;
        layout = topLevel->layout();
    }

    Block* build(Syntax* s) {
        assert(!block);
        if (parent) {
            assert(!topLevel);
            topLevel = parent->topLevel;
        } else if (!topLevel) {
            topLevel = new Object;
        }
        layout = DefinitionFinder::buildLayout(s, layout);
        if (!parent)
            topLevel->extend(layout);
        assert(topLevel);

        block = new Block(layout);
        s->accept(*this);
        Block* result = block;
        block = nullptr;
        return result;
    }

    Block* buildBody(Syntax* s) {
        Root<Block*> b(build(s));
        assert(b->instrCount() != 0);
        if (!b->lastInstr()->is<InstrReturn>()) {
            b->append(new InstrPop());
            b->append(new InstrConst(None));
            b->append(new InstrReturn());
        }
#ifdef TRACE_BUILD
        cerr << repr(b) << endl;
#endif
        return b;
    }

    Block* buildModule(Syntax* s) {
        Root<Block*> b(build(s));
        assert(b->instrCount() != 0);
        if (!b->lastInstr()->is<InstrReturn>()) {
            b->append(new InstrReturn());
        }
#ifdef TRACE_BUILD
        cerr << repr(b) << endl;
#endif
        return b;
    }

  private:
    BlockBuilder* parent;
    Root<Object*> topLevel;
    Root<Layout*> layout;
    Root<Block*> block;

    int lookupLexical(Name name) {
        int count = 1;
        BlockBuilder* b = parent;
        while (b && b->parent) {
            if (b->block->layout()->hasName(name))
                return count;
            ++count;
            b = b->parent;
        }
        return 0;
    }

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
        for (auto i = s.stmts().begin(); i != s.stmts().end(); ++i) {
            if (i != s.stmts().begin())
                block->append(new InstrPop());
            (*i)->accept(*this);
        }
    }

    virtual void visit(const SyntaxInteger& s) {
        Root<Value> v(Integer::get(s.value()));
        block->append(new InstrConst(v));
    }

    virtual void visit(const SyntaxString& s) {
        Root<Value> v(String::get(s.value()));
        block->append(new InstrConst(v));
    }

    virtual void visit(const SyntaxOr& s) {
        s.left()->accept(*this);
        unsigned branch = block->append(new InstrOr);
        s.right()->accept(*this);
        block->branchHere(branch);
    }

    virtual void visit(const SyntaxAnd& s) {
        s.left()->accept(*this);
        unsigned branch = block->append(new InstrAnd);
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

    virtual void visit(const SyntaxName& s) {
        Name name = s.id();
        if (parent && block->layout()->hasName(name))
            block->append(new InstrGetLocal(name));
        else if (int frame = lookupLexical(name))
            block->append(new InstrGetLexical(frame, name));
        else if (topLevel->hasAttr(name))
            block->append(new InstrGetGlobal(topLevel, name));
        else if (Builtin && Builtin->hasAttr(name))
            block->append(new InstrGetGlobal(Builtin, name));
        else
            throw ParseError(string("Name is not defined: ") + name);
    }

    virtual void visit(const SyntaxAssignName& s) {
        s.right()->accept(*this);
        Name name = s.left()->id();
        if (parent && block->layout()->hasName(name))
            block->append(new InstrSetLocal(name));
        else if (int frame = lookupLexical(name))
            block->append(new InstrSetLexical(frame, name));
        else if (topLevel->hasAttr(name))
            block->append(new InstrSetGlobal(topLevel, name));
        else if (Builtin && Builtin->hasAttr(name))
            block->append(new InstrSetGlobal(Builtin, name));
        else
            throw ParseError(string("Name is not defined: ") + name);
    }

    virtual void visit(const SyntaxAttrRef& s) {
        s.left()->accept(*this);
        block->append(new InstrGetAttr(s.right()->id()));
    }

    virtual void visit(const SyntaxAssignAttr& s) {
        s.right()->accept(*this);
        s.left()->left()->accept(*this);
        block->append(new InstrSetAttr(s.left()->right()->id()));
    }

    virtual void visit(const SyntaxCall& s) {
        // todo: check this actually how python works
        bool methodCall = s.left()->is<SyntaxAttrRef>();

        if (methodCall) {
            const SyntaxAttrRef* pr = s.left()->as<SyntaxAttrRef>();
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
        if (s.right())
            s.right()->accept(*this);
        else
            block->append(new InstrConst(None));
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

    virtual void visit(const SyntaxIf& s) {
        auto suites = s.branches();
        assert(suites.size() != 0);
        vector<unsigned> branchesToEnd;
        unsigned lastCondFailed = 0;
        for (unsigned i = 0; i != suites.size(); ++i) {
            if (i != 0)
                block->branchHere(lastCondFailed);
            suites[i].cond->accept(*this);
            lastCondFailed = block->append(new InstrBranchIfFalse);
            suites[i].block->accept(*this);
            branchesToEnd.push_back(block->append(new InstrBranchAlways));
        }
        block->branchHere(lastCondFailed);
        if (s.elseBranch())
            s.elseBranch()->accept(*this);
        else
            branchesToEnd.pop_back();
        for (unsigned i = 0; i < branchesToEnd.size(); ++i)
            block->branchHere(branchesToEnd[i]);
    }

    virtual void visit(const SyntaxWhile& s) {
        unsigned loopHead = block->nextIndex();
        s.cond()->accept(*this);
        unsigned branchToEnd = block->append(new InstrBranchIfFalse);
        s.suite()->accept(*this);
        block->append(new InstrBranchAlways(block->offsetTo(loopHead)));
        block->branchHere(branchToEnd);
        // todo: else
    }

    virtual void visit(const SyntaxLambda& a) {
        BlockBuilder exprBuilder(this);
        exprBuilder.setParams(a.params());
        Root<Block*> exprBlock(exprBuilder.build(a.expr()));
        exprBlock->append(new InstrReturn);
        block->append(new InstrLambda(a.params(), exprBlock));
    }

    virtual void visit(const SyntaxDef& s) {
        BlockBuilder exprBuilder(this);
        exprBuilder.setParams(s.params());
        Root<Block*> exprBlock(exprBuilder.buildBody(s.expr()));
        if (!exprBlock->lastInstr()->is<InstrReturn>()) {
            exprBlock->append(new InstrPop());
            exprBlock->append(new InstrConst(None));
        }
        block->append(new InstrLambda(s.params(), exprBlock));
        if (parent)
            block->append(new InstrSetLocal(s.id()));
        else
            block->append(new InstrSetGlobal(topLevel, s.id()));
    }
};

static unique_ptr<SyntaxBlock> ParseModule(const Input& input)
{
    SyntaxParser parser;
    parser.start(input);
    return unique_ptr<SyntaxBlock>(parser.parseModule());
}

Block* Block::buildModule(const Input& input, Traced<Object*> globals)
{
    unique_ptr<SyntaxBlock> syntax(ParseModule(input));
    BlockBuilder builder;
    if (globals)
        builder.setGlobals(globals);
    return builder.buildModule(syntax.get());
}

#ifdef BUILD_TESTS

#include "test.h"

void testBuildModule(const string& input, const string& expected)
{
    testEqual(repr(Block::buildModule(input, Object::Null)), expected);
}

testcase(block)
{
    testBuildModule("foo = 1\n"
                    "foo.bar",
                    "Const 1, SetGlobal foo, Pop, "
                    "GetGlobal foo, GetAttr bar, Return");

    testBuildModule("foo = 1\n"
                    "foo()",
                    "Const 1, SetGlobal foo, Pop, "
                    "GetGlobal foo, Call 0, Return");

    testBuildModule("foo = 1\n"
                    "bar = 1\n"
                    "baz = 1\n"
                    "foo(bar, baz)",
                    "Const 1, SetGlobal foo, Pop, "
                    "Const 1, SetGlobal bar, Pop, "
                    "Const 1, SetGlobal baz, Pop, "
                    "GetGlobal foo, GetGlobal bar, GetGlobal baz, Call 2, Return");

    testBuildModule("foo = 1\n"
                    "baz = 1\n"
                    "foo.bar(baz)",
                    "Const 1, SetGlobal foo, Pop, "
                    "Const 1, SetGlobal baz, Pop, "
                    "GetGlobal foo, GetMethod bar, GetGlobal baz, Call 2, Return");

    testBuildModule("foo = 1\n"
                    "baz = 1\n"
                    "foo.bar = baz",
                    "Const 1, SetGlobal foo, Pop, "
                    "Const 1, SetGlobal baz, Pop, "
                    "GetGlobal baz, GetGlobal foo, SetAttr bar, Return");

    testBuildModule("foo = 1\n"
                    "foo + 1",
                    "Const 1, SetGlobal foo, Pop, "
                    "GetGlobal foo, GetMethod __add__, Const 1, Call 2, Return");

    testBuildModule("1",
                    "Const 1, Return");

    testBuildModule("return 1",
                    "Const 1, Return");

    testBuildModule("foo = 1\n"
                    "bar = 1\n"
                    "return foo in bar",
                    "Const 1, SetGlobal foo, Pop, "
                    "Const 1, SetGlobal bar, Pop, "
                    "GetGlobal foo, GetGlobal bar, In, Return");

    testBuildModule("foo = 1\n"
                    "bar = 1\n"
                    "return foo is not bar",
                    "Const 1, SetGlobal foo, Pop, "
                    "Const 1, SetGlobal bar, Pop, "
                    "GetGlobal foo, GetGlobal bar, Is, Not, Return");

    testBuildModule("return 2 - - 1",
                    "Const 2, GetMethod __sub__, Const 1, GetMethod __neg__, Call 1, Call 2, Return");
}

#endif
