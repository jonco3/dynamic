#include "block.h"
#include "builtin.h"
#include "common.h"
#include "generator.h"
#include "instr.h"
#include "object.h"
#include "parser.h"
#include "repr.h"
#include "string.h"
#include "syntax.h"
#include "utils.h"

#include <algorithm>
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

int Block::offsetFrom(unsigned source)
{
    assert(source < instrs_.size() - 1);
    return instrs_.size() - source;
}

int Block::offsetTo(unsigned dest)
{
    assert(dest < instrs_.size() - 1);
    return dest - instrs_.size();
}

void Block::branchHere(unsigned source)
{
    assert(source < instrs_.size() - 1);
    Branch* b = instrs_[source]->asBranch();
    b->setOffset(offsetFrom(source));
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
    gc.trace(t, &layout_);
    for (auto i = instrs_.begin(); i != instrs_.end(); ++i)
        gc.trace(t, &*i);
}

TokenPos Block::getPos(Instr** instrp) const
{
    assert(contains(instrp));

    // We don't have any information for manually created blocks.
    if (offsetLines_.empty())
        return TokenPos();

    size_t offset = instrp - &instrs_.front();
    for (int i = offsetLines_.size() - 1; i >= 0; --i) {
        if (offsetLines_[i].first <= offset)
            return TokenPos(file_, offsetLines_[i].second, 0);
    }
    assert(false);
    return TokenPos();
}

void Block::setNextPos(const TokenPos& pos)
{
    if (offsetLines_.empty()) {
        file_ = pos.file;
        offsetLines_.push_back({0, pos.line});
        return;
    }

    assert(file_ == pos.file);
    if (offsetLines_.back().second != pos.line)
        offsetLines_.emplace_back(instrs_.size(), pos.line);
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
    DefinitionFinder(Layout* layout)
      : inAssignTarget_(false), layout_(layout)
    {}

    ~DefinitionFinder() {
        assert(!inAssignTarget_);
    }

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
        {
            assert(!inAssignTarget_);
            AutoSetAndRestore setInAssign(inAssignTarget_, true);
            s.left()->accept(*this);
        }
        s.right()->accept(*this);
    }

    virtual void visit(const SyntaxTargetList& s) {
        assert(inAssignTarget_);
        const auto& targets = s.targets();
        for (auto i = targets.begin(); i != targets.end(); i++)
            (*i)->accept(*this);
    }

    virtual void visit(const SyntaxName& s) {
        if (inAssignTarget_ &&
            find(globals_.begin(), globals_.end(), s.id()) == globals_.end())
        {
            addName(s.id());
        }
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
        {
            assert(!inAssignTarget_);
            AutoSetAndRestore setInAssign(inAssignTarget_, true);
            s.targets()->accept(*this);
        }
        s.suite()->accept(*this);
        if (s.elseSuite())
            s.elseSuite()->accept(*this);
    }

    virtual void visit(const SyntaxTry& s) {
        assert(!inAssignTarget_);
        s.trySuite()->accept(*this);
        for (const auto& except : s.excepts()) {
            if (except->as()) {
                AutoSetAndRestore setInAssign(inAssignTarget_, true);
                except->as()->accept(*this);
            }
            except->suite()->accept(*this);
        }
        if (s.elseSuite())
            s.elseSuite()->accept(*this);
        if (s.finallySuite())
            s.finallySuite()->accept(*this);
    }

  private:
    bool inAssignTarget_;
    Root<Layout*> layout_;
    vector<Name> globals_;
};

struct ByteCompiler : public SyntaxVisitor
{
    ByteCompiler()
      : parent(nullptr),
        topLevel(nullptr),
        layout(Frame::InitialLayout),
        block(nullptr),
        isClassBlock(false),
        isGenerator(false),
        inAssignTarget(false),
        loopHeadOffset(0)
    {}

    ~ByteCompiler() {
        assert(!inAssignTarget);
        assert(contextStack.empty());
        assert(loopHeadOffset == 0);
        assert(breakInstrs.empty());
    }

    Block* buildModule(Traced<Object*> globals, const Syntax* s) {
        assert(globals);
        topLevel = globals;
        layout = topLevel->layout();
        build(s);
        if (!block->lastInstr()->is<InstrReturn>())
            emit<InstrReturn>();
#ifdef TRACE_BUILD
        cerr << repr(*block) << endl;
#endif
        return block;
    }

    Block* buildFunctionBody(ByteCompiler* parent,
                             const vector<Parameter>& params,
                             const Syntax* s)
    {
        assert(parent);
        this->parent = parent;
        topLevel = parent->topLevel;
        for (auto i = params.begin(); i != params.end(); ++i)
            layout = layout->addName(i->name);
        build(s);
        if (!block->lastInstr()->is<InstrReturn>()) {
            emit<InstrPop>();
            emit<InstrConst>(None);
            emit<InstrReturn>();
        }
#ifdef TRACE_BUILD
        cerr << repr(*block) << endl;
#endif
        return block;
    }

    Block* buildLambda(ByteCompiler* parent, const vector<Parameter>& params,
                       const Syntax* s) {
        assert(parent);
        this->parent = parent;
        topLevel = parent->topLevel;
        for (auto i = params.begin(); i != params.end(); ++i)
            layout = layout->addName(i->name);
        build(s);
        emit<InstrReturn>();
#ifdef TRACE_BUILD
        cerr << repr(*block) << endl;
#endif
        return block;
    }

    Block* buildClass(ByteCompiler* parent, Name id, const Syntax* s) {
        assert(parent);
        this->parent = parent;
        topLevel = parent->topLevel;
        layout = layout->addName(ClassFunctionParam);
        isClassBlock = true;
        build(s);
        emit<InstrPop>();
        emit<InstrMakeClassFromFrame>(id);
        emit<InstrReturn>();
#ifdef TRACE_BUILD
        cerr << repr(*block) << endl;
#endif
        return block;
    }

    Block* buildGenerator(ByteCompiler* parent,
                          const vector<Parameter>& params,
                          const Syntax* s)
    {
        assert(parent);
        this->parent = parent;
        topLevel = parent->topLevel;
        isGenerator = true;
        for (auto i = params.begin(); i != params.end(); ++i)
            layout = layout->addName(i->name);
        build(s);
        if (!block->lastInstr()->is<InstrLeaveGenerator>()) {
            emit<InstrPop>();
            emit<InstrLeaveGenerator>();
        }
#ifdef TRACE_BUILD
        cerr << repr(*block) << endl;
#endif
        return block;
    }

  private:
    enum class Context {
        Loop,
        Finally
    };

    ByteCompiler* parent;
    Root<Object*> topLevel;
    Root<Layout*> layout;
    Root<Block*> block;
    bool isClassBlock;
    bool isGenerator;
    bool inAssignTarget;
    vector<Context> contextStack;
    unsigned loopHeadOffset;
    vector<unsigned> breakInstrs;
    TokenPos currentPos;

    using AutoPushContext = AutoPushStack<Context>;
    using AutoSetAndRestoreOffset = AutoSetAndRestoreValue<unsigned>;

    bool inLoop() {
        return contains(contextStack, Context::Loop);
    }

    template <typename T, typename... Args>
    unsigned emit(Args&& ...args) {
        return block->append(gc.create<T>(forward<Args>(args)...));
    }

    void compile(const Syntax* s) {
        s->accept(*this);
    }

    template <typename T>
    void compile(const unique_ptr<T>& s) {
        s->accept(*this);
    }

    void setBreakTargets() {
        unsigned pos = block->instrCount();
        for (unsigned source : breakInstrs) {
            InstrLoopControlJump *instr =
                block->instr(source)->as<InstrLoopControlJump>();
            instr->setTarget(pos);
        }
        breakInstrs.clear();
    }

    void maybeAssertStackDepth(unsigned expected) {
#if defined(DEBUG) && !defined(BUILD_TESTS)
        emit<InstrAssertStackDepth>(expected);
#endif
    }

    void build(const Syntax* s) {
        assert(!block);
        assert(topLevel);
        layout = DefinitionFinder::buildLayout(s, layout);
        if (!parent)
            topLevel->extend(layout);
        block = gc.create<Block>(layout);
        maybeAssertStackDepth(0);
        compile(s);
        maybeAssertStackDepth(1);
    }

    unsigned lookupLexical(Name name) {
        int count = 1;
        ByteCompiler* b = parent;
        while (b && b->parent) {
            if (b->block->layout()->hasName(name))
                return count;
            ++count;
            b = b->parent;
        }
        return 0;
    }

    void callUnaryMethod(const UnarySyntax& s, string name) {
        compile(s.right());
        emit<InstrGetMethod>(name);
        emit<InstrCall>(1);
    }

    template <typename BaseType>
    void callBinaryMethod(const BinarySyntax<BaseType, Syntax, Syntax>& s, string name) {
        compile(s.left());
        emit<InstrGetMethod>(name);
        compile(s.right());
        emit<InstrCall>(2);
    }

    virtual void visit(const SyntaxPass& s) {
        emit<InstrConst>(None);
    }

    virtual void visit(const SyntaxGlobal& s) {
        emit<InstrConst>(None);
    }

    virtual void visit(const SyntaxBlock& s) {
        if (s.stmts().empty()) {
            emit<InstrConst>(None);
            return;
        }

        for (auto i = s.stmts().begin(); i != s.stmts().end(); ++i) {
            if (i != s.stmts().begin())
                emit<InstrPop>();
            compile((*i));
        }
    }

    virtual void visit(const SyntaxInteger& s) {
        Root<Value> v(Integer::get(s.value()));
        emit<InstrConst>(v);
    }

    virtual void visit(const SyntaxString& s) {
        Root<Value> v(String::get(s.value()));
        emit<InstrConst>(v);
    }

    virtual void visit(const SyntaxExprList& s) {
        for (auto i = s.elems().begin(); i != s.elems().end(); ++i)
            compile((*i));
        emit<InstrTuple>(s.elems().size());
    }

    virtual void visit(const SyntaxList& s) {
        for (auto i = s.elems().begin(); i != s.elems().end(); ++i)
            compile((*i));
        emit<InstrList>(s.elems().size());
    }

    virtual void visit(const SyntaxDict& s) {
        for (auto i = s.entries().begin(); i != s.entries().end(); ++i) {
            compile(i->first);
            compile(i->second);
        }
        emit<InstrDict>(s.entries().size());
    }

    virtual void visit(const SyntaxOr& s) {
        compile(s.left());
        unsigned branch = emit<InstrOr>();
        compile(s.right());
        block->branchHere(branch);
    }

    virtual void visit(const SyntaxAnd& s) {
        compile(s.left());
        unsigned branch = emit<InstrAnd>();
        compile(s.right());
        block->branchHere(branch);
    }

    virtual void visit(const SyntaxNot& s) {
        compile(s.right());
        emit<InstrNot>();
    }

    virtual void visit(const SyntaxPos& s) { callUnaryMethod(s, "__pos__"); }
    virtual void visit(const SyntaxNeg& s) { callUnaryMethod(s, "__neg__"); }
    virtual void visit(const SyntaxInvert& s) { callUnaryMethod(s, "__invert__"); }

    virtual void visit(const SyntaxBinaryOp& s) {
        compile(s.left());
        compile(s.right());
        emit<InstrBinaryOp>(s.op());
    }

    virtual void visit(const SyntaxAugAssign& s) {
        // todo: what's __itruediv__?
        // todo: we'll need to change this if we want to specialise depending on
        // whether the target implements update-in-place methods
        compile(s.left());
        compile(s.right());
        emit<InstrAugAssignUpdate>(s.op());
        {
            assert(!inAssignTarget);
            AutoSetAndRestore setInAssign(inAssignTarget, true);
            compile(s.left());
        }
        emit<InstrPop>();
        emit<InstrConst>(None);
    }

    virtual void visit(const SyntaxCompareOp& s) {
        callBinaryMethod(s, CompareOpMethodNames[s.op()]);
    }


#define define_vist_binary_instr(syntax, instr)                               \
    virtual void visit(const syntax& s) {                                     \
        compile(s.left());                                              \
        compile(s.right());                                             \
        emit<instr>();                                   \
    }

    define_vist_binary_instr(SyntaxIn, InstrIn);
    define_vist_binary_instr(SyntaxIs, InstrIs);

#undef define_vist_binary_instr

    virtual void visit(const SyntaxAssign& s) {
        compile(s.right());
        assert(!inAssignTarget);
        AutoSetAndRestore setInAssign(inAssignTarget, true);
        compile(s.left());
    }

    virtual void visit(const SyntaxName& s) {
        Name name = s.id();
        if (inAssignTarget) {
            if (parent && block->layout()->hasName(name))
                emit<InstrSetLocal>(name);
            else if (unsigned frame = lookupLexical(name))
                emit<InstrSetLexical>(frame, name);
            else if (topLevel->hasAttr(name))
                emit<InstrSetGlobal>(topLevel, name);
            else if (Builtin && Builtin->hasAttr(name))
                emit<InstrSetGlobal>(Builtin, name);
            else
                throw ParseError(s.token(),
                                 string("Name is not defined: ") + name);
        } else {
            if (parent && block->layout()->hasName(name))
                emit<InstrGetLocal>(name);
            else if (unsigned frame = lookupLexical(name))
                emit<InstrGetLexical>(frame, name);
            else if (topLevel->hasAttr(name))
                emit<InstrGetGlobal>(topLevel, name);
            else if (Builtin && Builtin->hasAttr(name))
                emit<InstrGetGlobal>(Builtin, name);
            else
                throw ParseError(s.token(),
                                 string("Name is not defined: ") + name);
        }
    }

    virtual void visit(const SyntaxAttrRef& s) {
        bool wasInAssignTarget = inAssignTarget;
        AutoSetAndRestore clearInAssign(inAssignTarget, false);
        compile(s.left());
        Name id = s.right()->id();
        if (wasInAssignTarget)
            emit<InstrSetAttr>(id);
        else
            emit<InstrGetAttr>(id);
    }

    virtual void visit(const SyntaxSubscript& s) {
        bool wasInAssignTarget = inAssignTarget;
        AutoSetAndRestore clearInAssign(inAssignTarget, false);
        if (wasInAssignTarget) {
            compile(s.left());
            emit<InstrGetMethod>("__setitem__");
            compile(s.right());
            // todo: there may be a better way than this stack manipulation
            emit<InstrDup>(3);
            emit<InstrCall>(3);
            emit<InstrSwap>();
            emit<InstrPop>();
        } else {
            callBinaryMethod(s, "__getitem__");
        }
    }

    virtual void visit(const SyntaxTargetList& s) {
        assert(inAssignTarget);
        const auto& targets = s.targets();
        emit<InstrDestructure>(targets.size());
        for (unsigned i = 0; i < targets.size(); i++) {
            compile(targets[i]);
            if (i != targets.size() - 1)
                emit<InstrPop>();
        }
    }

    virtual void visit(const SyntaxCall& s) {
        // todo: check this actually how python works
        bool methodCall = s.left()->is<SyntaxAttrRef>();

        if (methodCall) {
            const SyntaxAttrRef* pr = s.left()->as<SyntaxAttrRef>();
            compile(pr->left());
            emit<InstrGetMethod>(pr->right()->id());
        } else {
            compile(s.left());
        }

        for (auto i = s.right().begin(); i != s.right().end(); ++i)
            compile((*i));
        unsigned count = s.right().size() + (methodCall ? 1 : 0);
        emit<InstrCall>(count);
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
            emit<InstrLeaveGenerator>();
        } else {
            if (s.right())
                compile(s.right());
            else
                emit<InstrConst>(None);
            emit<InstrReturn>();
        }
    }

    virtual void visit(const SyntaxCond& s) {
        compile(s.cond());
        unsigned altBranch = emit<InstrBranchIfFalse>();
        compile(s.cons());
        unsigned endBranch = emit<InstrBranchAlways>();
        block->branchHere(altBranch);
        compile(s.alt());
        block->branchHere(endBranch);
    }

    virtual void visit(const SyntaxIf& s) {
        const auto& suites = s.branches();
        assert(suites.size() != 0);
        vector<unsigned> branchesToEnd;
        unsigned lastCondFailed = 0;
        for (unsigned i = 0; i != suites.size(); ++i) {
            if (i != 0)
                block->branchHere(lastCondFailed);
            compile(suites[i].cond);
            lastCondFailed = emit<InstrBranchIfFalse>();
            compile(suites[i].block);
            if (s.elseBranch() || i != suites.size() - 1)
                branchesToEnd.push_back(emit<InstrBranchAlways>());
        }
        block->branchHere(lastCondFailed);
        if (s.elseBranch())
            compile(s.elseBranch());
        for (unsigned i = 0; i < branchesToEnd.size(); ++i)
            block->branchHere(branchesToEnd[i]);
        emit<InstrConst>(None);
    }

    virtual void visit(const SyntaxWhile& s) {
        AutoSetAndRestoreOffset setLoopHead(loopHeadOffset, block->nextIndex());
        vector<unsigned> oldBreakInstrs = move(breakInstrs);
        breakInstrs.clear();
        compile(s.cond());
        unsigned branchToEnd = emit<InstrBranchIfFalse>();
        {
            AutoPushContext enterLoop(contextStack, Context::Loop);
            compile(s.suite());
        }
        emit<InstrPop>();
        emit<InstrBranchAlways>(block->offsetTo(loopHeadOffset));
        block->branchHere(branchToEnd);
        setBreakTargets();
        breakInstrs = move(oldBreakInstrs);
        emit<InstrConst>(None);
        // todo: else
    }

    virtual void visit(const SyntaxFor& s) {
        // 1. Get iterator
        compile(s.exprs());
        emit<InstrGetMethod>("__iter__");
        emit<InstrCall>(1);
        emit<InstrGetMethod>("next");

        // 2. Call next on iterator and break if end (loop heap)
        AutoSetAndRestoreOffset setLoopHead(loopHeadOffset, block->nextIndex());
        vector<unsigned> oldBreakInstrs = move(breakInstrs);
        breakInstrs.clear();
        emit<InstrIteratorNext>();
        unsigned exitBranch = emit<InstrBranchIfFalse>();

        // 3. Assign results
        {
            assert(!inAssignTarget);
            AutoSetAndRestore setInAssign(inAssignTarget, true);
            compile(s.targets());
        }
        emit<InstrPop>();

        // 4. Execute loop body
        {
            AutoPushContext enterLoop(contextStack, Context::Loop);
            compile(s.suite());
        }
        emit<InstrPop>();
        emit<InstrBranchAlways>(block->offsetTo(loopHeadOffset));

        // 5. Exit
        block->branchHere(exitBranch);
        setBreakTargets();
        breakInstrs = move(oldBreakInstrs);
        emit<InstrPop>();
        emit<InstrPop>();
        emit<InstrConst>(None);

        // todo: else clause
    }

    void emitLambda(Name defName, const vector<Parameter>& params,
                    Traced<Block*> exprBlock, bool isGenerator)
    {
        vector<Name> names;
        unsigned defaultCount = 0;
        for (auto i = params.begin(); i != params.end(); i++) {
            names.push_back(i->name);
            if (i->maybeDefault) {
                defaultCount++;
                compile(i->maybeDefault);
            }
        }
        bool takesRest = false;
        if (params.size() > 0)
            takesRest = params.back().takesRest;
        emit<InstrLambda>(defName, names, exprBlock, defaultCount, takesRest,
                          isGenerator);
    }

    virtual void visit(const SyntaxLambda& s) {
        Root<Block*> exprBlock(
            ByteCompiler().buildLambda(this, s.params(), s.expr()));
        emitLambda("(lambda)", s.params(), exprBlock, false);
    }

    virtual void visit(const SyntaxDef& s) {
        Root<Block*> exprBlock;
        if (s.isGenerator()) {
            exprBlock =
                ByteCompiler().buildGenerator(this, s.params(), s.expr());
        } else {
            exprBlock =
                ByteCompiler().buildFunctionBody(this, s.params(), s.expr());
        }
        emitLambda(s.id(), s.params(), exprBlock, s.isGenerator());
        if (parent)
            emit<InstrSetLocal>(s.id());
        else
            emit<InstrSetGlobal>(topLevel, s.id());
    }

    virtual void visit(const SyntaxClass& s) {
        Root<Block*> suite(ByteCompiler().buildClass(this, s.id(), s.suite()));
        vector<Name> params = { ClassFunctionParam };
        emit<InstrLambda>(s.id(), params, suite);
        compile(s.bases());
        emit<InstrCall>(1);
        if (parent)
            emit<InstrSetLocal>(s.id());
        else
            emit<InstrSetGlobal>(topLevel, s.id());
    }

    virtual void visit(const SyntaxAssert& s) {
        if (debugMode) {
            compile(s.cond());
            unsigned endBranch = emit<InstrBranchIfTrue>();
            if (s.message()) {
                compile(s.message());
                emit<InstrGetMethod>("__str__");
                emit<InstrCall>(1);
            } else {
                emit<InstrConst>(None);
            }
            emit<InstrAssertionFailed>();
            block->branchHere(endBranch);
        }
        emit<InstrConst>(None);
    }

    virtual void visit(const SyntaxRaise& s) {
        compile(s.right());
        emit<InstrRaise>();
    }

    virtual void visit(const SyntaxYield& s) {
        compile(s.right());
        emit<InstrSuspendGenerator>();
    }

    virtual void visit(const SyntaxTry& s) {
        unsigned finallyBranch = 0;
        if (s.finallySuite()) {
            contextStack.push_back(Context::Finally);
            finallyBranch = emit<InstrEnterFinallyRegion>();
        }
        if (s.excepts().size() != 0) {
            emitTryCatch(s);
        } else {
            compile(s.trySuite());
            emit<InstrPop>();
        }
        if (s.finallySuite()) {
            emit<InstrLeaveFinallyRegion>();
            block->branchHere(finallyBranch);
            compile(s.finallySuite());
            emit<InstrPop>();
            emit<InstrFinishExceptionHandler>();
            assert(contextStack.back() == Context::Finally);
            contextStack.pop_back();
        }
        emit<InstrConst>(None);
    }

    virtual void emitTryCatch(const SyntaxTry& s) {
        unsigned handlerBranch =
            emit<InstrEnterCatchRegion>();
        compile(s.trySuite());
        emit<InstrPop>();
        emit<InstrLeaveCatchRegion>();
        vector<unsigned> handledBranches;
        for (const auto& e : s.excepts()) {
            handledBranches.push_back(
                emit<InstrBranchAlways>());
            block->branchHere(handlerBranch);
            compile(e->expr());
            emit<InstrMatchCurrentException>();
            handlerBranch = emit<InstrBranchIfFalse>();
            if (e->as()) {
                assert(!inAssignTarget);
                AutoSetAndRestore setInAssign(inAssignTarget, true);
                compile(e->as());
            }
            emit<InstrPop>();
            compile(e->suite());
            emit<InstrPop>();
        }
        if (s.elseSuite()) {
            handledBranches.push_back(
                emit<InstrBranchAlways>());
            block->branchHere(handlerBranch);
            compile(s.elseSuite());
            emit<InstrPop>();
        }
        block->branchHere(handlerBranch);
        emit<InstrFinishExceptionHandler>();
        for (unsigned offset: handledBranches)
            block->branchHere(offset);
    }

    enum class LoopExit {
        Break,
        Continue
    };

    unsigned countFinallyBlocksInLoop() {
        unsigned count = 0;
        for (auto i = contextStack.rbegin(); i != contextStack.rend(); i++) {
            if (*i == Context::Finally)
                count++;
            else if (*i == Context::Loop)
                break;
        }
        return count;
    }

    virtual void visit(const SyntaxBreak& s) {
        if (!inLoop())
            throw ParseError(s.token(), "SyntaxError: 'break' outside loop");
        unsigned finallyCount = countFinallyBlocksInLoop();
        unsigned offset = emit<InstrLoopControlJump>(finallyCount);
        breakInstrs.push_back(offset);
    }

    virtual void visit(const SyntaxContinue& s) {
        if (!inLoop())
            throw ParseError(s.token(), "SyntaxError: 'continue' outside loop");
        unsigned finallyCount = countFinallyBlocksInLoop();
        emit<InstrLoopControlJump>(finallyCount, loopHeadOffset);
    }

    virtual void setPos(const TokenPos& pos) {
        block->setNextPos(pos);
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
    if (globals->isNone())
        globals = gc.create<Object>();
    blockOut = ByteCompiler().buildModule(globals, syntax.get());
}

#ifdef BUILD_TESTS

#include "test.h"

void testBuildModule(const string& input, const string& expected)
{
    Root<Block*> block;
    Block::buildModule(input, None, block);
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
