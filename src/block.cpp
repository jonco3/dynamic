#include "block.h"
#include "builtin.h"
#include "common.h"
#include "generator.h"
#include "instr.h"
#include "parser.h"
#include "repr.h"
#include "string.h"
#include "syntax.h"

#include <memory>

//#define TRACE_BUILD

static const Name ClassFunctionParam = "__bases__";

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

#ifdef BUILD_TESTS
Instr** Block::findInstr(unsigned type)
{
    for (auto i = instrs_.begin(); i != instrs_.end(); ++i) {
        if ((*i)->type() == type)
            return &(*i);
    }
    return nullptr;
}
#endif

// Find the names that are defined in the current block
struct DefinitionFinder : public DefaultSyntaxVisitor
{
    DefinitionFinder(Layout* layout) : inAssignTarget_(false), layout_(layout) {}

    static Layout* buildLayout(const Syntax *s, Layout* layout) {
        DefinitionFinder df(layout);
        s->accept(df);
        return df.layout_;
    }

    void addName(Name name) {
        if (find(globals_.begin(), globals_.end(), name) == globals_.end())
            layout_ = layout_->maybeAddName(name);
    }

    // Record assignments and definintions

    virtual void visit(const SyntaxAssign& s) {
        assert(!inAssignTarget_);
        inAssignTarget_ = true;
        s.left()->accept(*this);
        inAssignTarget_ = false;
    }

    virtual void visit(const SyntaxTargetList& s) {
        assert(inAssignTarget_);
        const auto& targets = s.targets();
        for (auto i = targets.begin(); i != targets.end(); i++)
            (*i)->accept(*this);
    }

    virtual void visit(const SyntaxName& s) {
        // todo: check this is not a global ref
        if (inAssignTarget_)
            addName(s.id());
    }

    virtual void visit(const SyntaxDef& s) {
        addName(s.id());
    }

    virtual void visit(const SyntaxClass& s) {
        addName(s.id());
    }

    virtual void visit(const SyntaxGlobal& s) {
        const vector<Name>& names = s.names();
        for (auto i = names.begin(); i != names.end(); i++) {
            if (layout_->hasName(*i)) {
                throw ParseError(s.token(),
                    "name " + *i + " is assigned to before global declaration");
            }
        }
        globals_.insert(globals_.end(), names.begin(), names.end());
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

    virtual void visit(const SyntaxFor& s) {
        assert(!inAssignTarget_);
        inAssignTarget_ = true;
        s.targets()->accept(*this);
        inAssignTarget_ = false;
        s.suite()->accept(*this);
        if (s.elseSuite())
            s.elseSuite()->accept(*this);
    }

  private:
    bool inAssignTarget_;
    Root<Layout*> layout_;
    vector<Name> globals_;
};

struct BlockBuilder : public SyntaxVisitor
{
    BlockBuilder()
      : parent(nullptr),
        topLevel(nullptr),
        layout(Frame::InitialLayout),
        block(nullptr),
        isClassBlock(false),
        isGenerator(false),
        inAssignTarget(false)
    {}

    Block* buildModule(Traced<Object*> globals, const Syntax* s) {
        assert(globals);
        topLevel = globals;
        layout = topLevel->layout();
        build(s);
        if (!block->lastInstr()->is<InstrReturn>())
            block->append(gc::create<InstrReturn>());
#ifdef TRACE_BUILD
        cerr << repr(*block) << endl;
#endif
        return block;
    }

    Block* buildFunctionBody(BlockBuilder* parent,
                             const vector<Name>& params,
                             const Syntax* s)
    {
        assert(parent);
        this->parent = parent;
        topLevel = parent->topLevel;
        for (auto i = params.begin(); i != params.end(); ++i)
            layout = layout->addName(*i);
        build(s);
        if (!block->lastInstr()->is<InstrReturn>()) {
            block->append(gc::create<InstrPop>());
            block->append(gc::create<InstrConst>(None));
            block->append(gc::create<InstrReturn>());
        }
#ifdef TRACE_BUILD
        cerr << repr(*block) << endl;
#endif
        return block;
    }

    Block* buildLambda(BlockBuilder* parent, const vector<Name>& params,
                       const Syntax* s) {
        assert(parent);
        this->parent = parent;
        topLevel = parent->topLevel;
        for (auto i = params.begin(); i != params.end(); ++i)
            layout = layout->addName(*i);
        build(s);
#ifdef TRACE_BUILD
        cerr << repr(*block) << endl;
#endif
        return block;
    }

    Block* buildClass(BlockBuilder* parent, Name id, const Syntax* s) {
        assert(parent);
        this->parent = parent;
        topLevel = parent->topLevel;
        layout = layout->addName(ClassFunctionParam);
        isClassBlock = true;
        build(s);
        block->append(gc::create<InstrPop>());
        block->append(gc::create<InstrMakeClassFromFrame>(id));
        block->append(gc::create<InstrReturn>());
#ifdef TRACE_BUILD
        cerr << repr(*block) << endl;
#endif
        return block;
    }

    Block* buildGenerator(BlockBuilder* parent,
                          const vector<Name>& params,
                          const Syntax* s)
    {
        assert(parent);
        this->parent = parent;
        topLevel = parent->topLevel;
        isGenerator = true;
        for (auto i = params.begin(); i != params.end(); ++i)
            layout = layout->addName(*i);
        build(s);
        if (!block->lastInstr()->is<InstrLeaveGenerator>()) {
            block->append(gc::create<InstrPop>());
            block->append(gc::create<InstrLeaveGenerator>());
        }
#ifdef TRACE_BUILD
        cerr << repr(*block) << endl;
#endif
        return block;
    }

  private:
    BlockBuilder* parent;
    Root<Object*> topLevel;
    Root<Layout*> layout;
    Root<Block*> block;
    bool isClassBlock;
    bool isGenerator;
    bool inAssignTarget;

    void build(const Syntax* s) {
        assert(!block);
        assert(topLevel);
        layout = DefinitionFinder::buildLayout(s, layout);
        if (!parent)
            topLevel->extend(layout);
        block = gc::create<Block>(layout);
        s->accept(*this);
    }

    bool setInAssignTarget(bool newValue) {
        bool oldValue = inAssignTarget;
        inAssignTarget = newValue;
        return oldValue;
    }

    unsigned lookupLexical(Name name) {
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
        block->append(gc::create<InstrGetMethod>(name));
        block->append(gc::create<InstrCall>(1));
    }

    template <typename BaseType>
    void callBinaryMethod(const BinarySyntax<BaseType, Syntax, Syntax>& s, string name) {
        s.left()->accept(*this);
        block->append(gc::create<InstrGetMethod>(name));
        s.right()->accept(*this);
        block->append(gc::create<InstrCall>(2));
        // todo: things that implement __rfoo__ rather than __foo__
    }

    virtual void visit(const SyntaxPass& s) {
        block->append(gc::create<InstrConst>(None));
    }

    virtual void visit(const SyntaxGlobal& s) {
        block->append(gc::create<InstrConst>(None));
    }

    virtual void visit(const SyntaxBlock& s) {
        for (auto i = s.stmts().begin(); i != s.stmts().end(); ++i) {
            if (i != s.stmts().begin())
                block->append(gc::create<InstrPop>());
            (*i)->accept(*this);
        }
    }

    virtual void visit(const SyntaxInteger& s) {
        Root<Value> v(Integer::get(s.value()));
        block->append(gc::create<InstrConst>(v));
    }

    virtual void visit(const SyntaxString& s) {
        Root<Value> v(String::get(s.value()));
        block->append(gc::create<InstrConst>(v));
    }

    virtual void visit(const SyntaxExprList& s) {
        for (auto i = s.elems().begin(); i != s.elems().end(); ++i)
            (*i)->accept(*this);
        block->append(gc::create<InstrTuple>(s.elems().size()));
    }

    virtual void visit(const SyntaxList& s) {
        for (auto i = s.elems().begin(); i != s.elems().end(); ++i)
            (*i)->accept(*this);
        block->append(gc::create<InstrList>(s.elems().size()));
    }

    virtual void visit(const SyntaxDict& s) {
        for (auto i = s.entries().begin(); i != s.entries().end(); ++i) {
            i->first->accept(*this);
            i->second->accept(*this);
        }
        block->append(gc::create<InstrDict>(s.entries().size()));
    }

    virtual void visit(const SyntaxOr& s) {
        s.left()->accept(*this);
        unsigned branch = block->append(gc::create<InstrOr>());
        s.right()->accept(*this);
        block->branchHere(branch);
    }

    virtual void visit(const SyntaxAnd& s) {
        s.left()->accept(*this);
        unsigned branch = block->append(gc::create<InstrAnd>());
        s.right()->accept(*this);
        block->branchHere(branch);
    }

    virtual void visit(const SyntaxNot& s) {
        s.right()->accept(*this);
        block->append(gc::create<InstrNot>());
    }

    virtual void visit(const SyntaxPos& s) { callUnaryMethod(s, "__pos__"); }
    virtual void visit(const SyntaxNeg& s) { callUnaryMethod(s, "__neg__"); }
    virtual void visit(const SyntaxInvert& s) { callUnaryMethod(s, "__invert__"); }

    virtual void visit(const SyntaxBinaryOp& s) {
        s.left()->accept(*this);
        s.right()->accept(*this);
        block->append(gc::create<InstrBinaryOp>(s.op()));
    }

    virtual void visit(const SyntaxAugAssign& s) {
        // todo: what's __itruediv__?
        // todo: we'll need to change this if we want to specialise depending on
        // whether the target implements update-in-place methods
        s.left()->accept(*this);
        s.right()->accept(*this);
        block->append(gc::create<InstrAugAssignUpdate>(s.op()));
        assert(!inAssignTarget);
        inAssignTarget = true;
        s.left()->accept(*this);
        inAssignTarget = false;
        block->append(gc::create<InstrPop>());
        block->append(gc::create<InstrConst>(None));
    }

    virtual void visit(const SyntaxCompareOp& s) {
        callBinaryMethod(s, CompareOpMethodNames[s.op()]);
    }


#define define_vist_binary_instr(syntax, instr)                               \
    virtual void visit(const syntax& s) {                                     \
        s.left()->accept(*this);                                              \
        s.right()->accept(*this);                                             \
        block->append(gc::create<instr>());                                   \
    }

    define_vist_binary_instr(SyntaxIn, InstrIn);
    define_vist_binary_instr(SyntaxIs, InstrIs);

#undef define_vist_binary_instr

    virtual void visit(const SyntaxAssign& s) {
        s.right()->accept(*this);
        assert(!inAssignTarget); // todo: python accepts a = b = 1
        inAssignTarget = true;
        s.left()->accept(*this);
        inAssignTarget = false;
    }

    virtual void visit(const SyntaxName& s) {
        Name name = s.id();
        if (inAssignTarget) {
            if (parent && block->layout()->hasName(name))
                block->append(gc::create<InstrSetLocal>(name));
            else if (unsigned frame = lookupLexical(name))
                block->append(gc::create<InstrSetLexical>(frame, name));
            else if (topLevel->hasAttr(name))
                block->append(gc::create<InstrSetGlobal>(topLevel, name));
            else if (Builtin && Builtin->hasAttr(name))
                block->append(gc::create<InstrSetGlobal>(Builtin, name));
            else
                throw ParseError(s.token(),
                                 string("Name is not defined: ") + name);
        } else {
            if (parent && block->layout()->hasName(name))
                block->append(gc::create<InstrGetLocal>(name));
            else if (unsigned frame = lookupLexical(name))
                block->append(gc::create<InstrGetLexical>(frame, name));
            else if (topLevel->hasAttr(name))
                block->append(gc::create<InstrGetGlobal>(topLevel, name));
            else if (Builtin && Builtin->hasAttr(name))
                block->append(gc::create<InstrGetGlobal>(Builtin, name));
            else
                throw ParseError(s.token(),
                                 string("Name is not defined: ") + name);
        }
    }

    virtual void visit(const SyntaxAttrRef& s) {
        bool wasInAssignTarget = setInAssignTarget(false);
        s.left()->accept(*this);
        Name id = s.right()->id();
        if (wasInAssignTarget)
            block->append(gc::create<InstrSetAttr>(id));
        else
            block->append(gc::create<InstrGetAttr>(id));
        setInAssignTarget(wasInAssignTarget);
    }

    virtual void visit(const SyntaxSubscript& s) {
        bool wasInAssignTarget = setInAssignTarget(false);
        if (wasInAssignTarget) {
            s.left()->accept(*this);
            block->append(gc::create<InstrGetMethod>("__setitem__"));
            s.right()->accept(*this);
            // todo: there may be a better way than this stack manipulation
            block->append(gc::create<InstrDup>(3));
            block->append(gc::create<InstrCall>(3));
            block->append(gc::create<InstrSwap>());
            block->append(gc::create<InstrPop>());
        } else {
            callBinaryMethod(s, "__getitem__");
        }
        setInAssignTarget(wasInAssignTarget);
    }

    virtual void visit(const SyntaxTargetList& s) {
        assert(inAssignTarget);
        const auto& targets = s.targets();
        block->append(gc::create<InstrDestructure>(targets.size()));
        for (unsigned i = 0; i < targets.size(); i++) {
            targets[i]->accept(*this);
            if (i != targets.size() - 1)
                block->append(gc::create<InstrPop>());
        }
    }

    virtual void visit(const SyntaxCall& s) {
        // todo: check this actually how python works
        bool methodCall = s.left()->is<SyntaxAttrRef>();

        if (methodCall) {
            const SyntaxAttrRef* pr = s.left()->as<SyntaxAttrRef>();
            pr->left()->accept(*this);
            block->append(gc::create<InstrGetMethod>(pr->right()->id()));
        } else {
            s.left()->accept(*this);
        }

        for (auto i = s.right().begin(); i != s.right().end(); ++i)
            (*i)->accept(*this);
        unsigned count = s.right().size() + (methodCall ? 1 : 0);
        block->append(gc::create<InstrCall>(count));
    }

    virtual void visit(const SyntaxReturn& s) {
        if (isClassBlock) {
            throw ParseError(s.token(),
                             "Return statement not allowed in class body");
        } else if (isGenerator) {
            if (s.right()) {
                throw ParseError(s.token(),
                                 "SyntaxError: 'return' with argument inside generator");
            }
            block->append(gc::create<InstrLeaveGenerator>());
        } else {
            if (s.right())
                s.right()->accept(*this);
            else
                block->append(gc::create<InstrConst>(None));
            block->append(gc::create<InstrReturn>());
        }
    }

    virtual void visit(const SyntaxCond& s) {
        s.cond()->accept(*this);
        unsigned altBranch = block->append(gc::create<InstrBranchIfFalse>());
        s.cons()->accept(*this);
        unsigned endBranch = block->append(gc::create<InstrBranchAlways>());
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
            lastCondFailed = block->append(gc::create<InstrBranchIfFalse>());
            suites[i].block->accept(*this);
            if (s.elseBranch() || i != suites.size() - 1)
                branchesToEnd.push_back(block->append(gc::create<InstrBranchAlways>()));
        }
        block->branchHere(lastCondFailed);
        if (s.elseBranch())
            s.elseBranch()->accept(*this);
        for (unsigned i = 0; i < branchesToEnd.size(); ++i)
            block->branchHere(branchesToEnd[i]);
        block->append(gc::create<InstrConst>(None));
    }

    virtual void visit(const SyntaxWhile& s) {
        unsigned loopHead = block->nextIndex();
        s.cond()->accept(*this);
        unsigned branchToEnd = block->append(gc::create<InstrBranchIfFalse>());
        s.suite()->accept(*this);
        block->append(gc::create<InstrPop>());
        block->append(gc::create<InstrBranchAlways>(block->offsetTo(loopHead)));
        block->branchHere(branchToEnd);
        block->append(gc::create<InstrConst>(None));
        // todo: else
    }

    virtual void visit(const SyntaxFor& s) {
        // 1. Get iterator
        s.exprs()->accept(*this);
        block->append(gc::create<InstrGetMethod>("__iter__"));
        block->append(gc::create<InstrCall>(1));
        block->append(gc::create<InstrGetMethod>("next"));

        // 2. Call next on iterator and break if end (loop heap)
        unsigned loopHead = block->append(gc::create<InstrIteratorNext>());
        unsigned exitBranch = block->append(gc::create<InstrBranchIfFalse>());

        // 3. Assign results
        assert(!inAssignTarget);
        inAssignTarget = true;
        s.targets()->accept(*this);
        inAssignTarget = false;
        block->append(gc::create<InstrPop>());

        // 4. Execute loop body
        s.suite()->accept(*this);
        block->append(gc::create<InstrPop>());
        block->append(gc::create<InstrBranchAlways>(block->offsetTo(loopHead)));

        // 5. Exit
        block->branchHere(exitBranch);
        block->append(gc::create<InstrPop>());
        block->append(gc::create<InstrPop>());
        block->append(gc::create<InstrConst>(None));

        // todo: else clause
    }

    virtual void visit(const SyntaxLambda& a) {
        Root<Block*> exprBlock(BlockBuilder().buildLambda(this, a.params(), a.expr()));
        exprBlock->append(gc::create<InstrReturn>());
        block->append(gc::create<InstrLambda>(a.params(), exprBlock));
    }

    virtual void visit(const SyntaxDef& s) {
        Root<Block*> exprBlock;
        if (s.isGenerator()) {
            exprBlock =
                BlockBuilder().buildGenerator(this, s.params(), s.expr());
        } else {
            exprBlock =
                BlockBuilder().buildFunctionBody(this, s.params(), s.expr());
        }
        block->append(
            gc::create<InstrLambda>(s.params(), exprBlock, s.isGenerator()));
        if (parent)
            block->append(gc::create<InstrSetLocal>(s.id()));
        else
            block->append(gc::create<InstrSetGlobal>(topLevel, s.id()));
    }

    virtual void visit(const SyntaxClass& s) {
        Root<Block*> suite(BlockBuilder().buildClass(this, s.id(), s.suite()));
        suite->append(gc::create<InstrPop>());
        suite->append(gc::create<InstrMakeClassFromFrame>(s.id()));
        suite->append(gc::create<InstrReturn>());

        vector<Name> params = { ClassFunctionParam };
        block->append(gc::create<InstrLambda>(params, suite));
        s.bases()->accept(*this);
        block->append(gc::create<InstrCall>(1));
        if (parent)
            block->append(gc::create<InstrSetLocal>(s.id()));
        else
            block->append(gc::create<InstrSetGlobal>(topLevel, s.id()));
    }

    virtual void visit(const SyntaxAssert& s) {
        if (debugMode) {
            s.cond()->accept(*this);
            unsigned endBranch = block->append(gc::create<InstrBranchIfTrue>());
            if (s.message()) {
                s.message()->accept(*this);
                block->append(gc::create<InstrGetMethod>("__str__"));
                block->append(gc::create<InstrCall>(1));
            } else {
                block->append(gc::create<InstrConst>(None));
            }
            block->append(gc::create<InstrAssertionFailed>());
            block->branchHere(endBranch);
        }
        block->append(gc::create<InstrConst>(None));
    }

    virtual void visit(const SyntaxRaise& s) {
        s.right()->accept(*this);
        block->append(gc::create<InstrRaise>());
    }

    virtual void visit(const SyntaxYield& s) {
        s.right()->accept(*this);
        block->append(gc::create<InstrSuspendGenerator>());
    }
};

static unique_ptr<SyntaxBlock> ParseModule(const Input& input)
{
    SyntaxParser parser;
    parser.start(input);
    return unique_ptr<SyntaxBlock>(parser.parseModule());
}

void Block::buildModule(const Input& input, Traced<Object*> globalsArg,
                        Root<Block*>& blockOut)
{
    unique_ptr<SyntaxBlock> syntax(ParseModule(input));
    Root<Object*> globals(globalsArg);
    if (!globals)
        globals = gc::create<Object>();
    blockOut = BlockBuilder().buildModule(globals, syntax.get());
}

#ifdef BUILD_TESTS

#include "test.h"

void testBuildModule(const string& input, const string& expected)
{
    Root<Block*> block;
    Block::buildModule(input, Object::Null, block);
    testEqual(repr(*block), expected);
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
                    "GetGlobal foo, Const 1, BinaryOp +, Return");

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
                    "Const 2, Const 1, GetMethod __neg__, Call 1, BinaryOp -, Return");
}

#endif
