#include "analysis.h"
#include "common.h"
#include "compiler.h"
#include "frame.h"
#include "instr.h"
#include "parser.h"
#include "repr.h"
#include "utils.h"

#ifdef DEBUG

bool assertStackDepth = true;
bool logCompile = false;

static inline void log(const Block* block) {
    if (logCompile)
        cout << "bc: " << repr(*block) << endl;
}

#else

static inline void log(const Block* block) {}

#endif

struct ByteCompiler : public SyntaxVisitor
{
    ByteCompiler()
      : parent(nullptr),
        topLevel(nullptr),
        layout(Env::InitialLayout),
        useLexicalEnv(false),
        block(nullptr),
        isClassBlock(false),
        isGenerator(false),
        contextStack({Context::None}),
        loopHeadOffset(0),
        stackDepth(0)
    {}

    ~ByteCompiler() {
        assert(contextStack.size() == 1);
        assert(contextStack.back() == Context::None);
        assert(loopHeadOffset == 0);
        assert(breakInstrs.empty());
    }

    Block* buildModule(Traced<Object*> globals, const Syntax* s) {
        assert(globals);
        topLevel = globals;
        layout = topLevel->layout();
        build(*s, 0);
        if (!block->lastInstr().data->is<InstrReturn>())
            emit<InstrReturn>();
        log(block);
        return block;
    }

    Block* buildFunctionBody(ByteCompiler* parent,
                             const vector<Parameter>& params,
                             const Syntax& s)
    {
        assert(parent);
        this->parent = parent;
        topLevel = parent->topLevel;
        for (auto i = params.begin(); i != params.end(); ++i)
            layout = layout->addName(i->name);
        build(s, params.size());
        if (!block->lastInstr().data->is<InstrReturn>()) {
            emit<InstrPop>();
            emit<InstrConst>(None);
            emit<InstrReturn>();
        }
        log(block);
        return block;
    }

    Block* buildLambda(ByteCompiler* parent, const vector<Parameter>& params,
                       const Syntax& s) {
        assert(parent);
        this->parent = parent;
        topLevel = parent->topLevel;
        for (auto i = params.begin(); i != params.end(); ++i)
            layout = layout->addName(i->name);
        build(s, params.size());
        emit<InstrReturn>();
        log(block);
        return block;
    }

    Block* buildClass(ByteCompiler* parent, Name id, const Syntax& s) {
        assert(parent);
        this->parent = parent;
        topLevel = parent->topLevel;
        layout = layout->addName(Name::__bases__);
        isClassBlock = true;
        useLexicalEnv = true;
        build(s, 1);
        emit<InstrPop>();
        emit<InstrMakeClassFromFrame>(id);
        emit<InstrReturn>();
        log(block);
        return block;
    }

    Block* buildGenerator(ByteCompiler* parent,
                          const vector<Parameter>& params,
                          const Syntax& s)
    {
        assert(parent);
        this->parent = parent;
        topLevel = parent->topLevel;
        isGenerator = true;
        for (auto i = params.begin(); i != params.end(); ++i)
            layout = layout->addName(i->name);
        build(s, params.size());
        if (!block->lastInstr().data->is<InstrLeaveGenerator>()) {
            emit<InstrPop>();
            emit<InstrLeaveGenerator>();
        }
        log(block);
        return block;
    }

  private:
    enum class Context {
        None,
        Loop,
        Finally,
        Assign,
        Delete
    };

    ByteCompiler* parent;
    Root<Object*> topLevel;
    Root<Layout*> layout;
    Root<Object*> defs;
    vector<Name> globals;
    bool useLexicalEnv;
    Root<Block*> block;
    bool isClassBlock;
    bool isGenerator;
    vector<Context> contextStack;
    unsigned loopHeadOffset;
    vector<unsigned> breakInstrs;
    unsigned stackDepth;
    TokenPos currentPos;

    using AutoPushContext = AutoPushStack<Context>;
    using AutoSetAndRestoreOffset = AutoSetAndRestoreValue<unsigned>;

    bool inLoop() {
        return contains(contextStack, Context::Loop);
    }

    bool inTarget() {
        return contextStack.back() == Context::Assign ||
               contextStack.back() == Context::Delete;
    }

    template <typename T, typename... Args>
    unsigned emit(Args&& ...args) {
        return block->append<T>(forward<Args>(args)...);
    }

    void compile(const Syntax* s) {
        compile(*s);
    }

    void compile(const Syntax& s) {
        s.accept(*this);
    }

    template <typename T>
    void compile(const unique_ptr<T>& s) {
        s->accept(*this);
    }

    void setBreakTargets() {
        unsigned pos = block->instrCount();
        for (unsigned source : breakInstrs) {
            InstrLoopControlJump *instr =
                block->instr(source).data->as<InstrLoopControlJump>();
            instr->setTarget(pos);
        }
        breakInstrs.clear();
    }

    void maybeAssertStackDepth(unsigned delta = 0) {
#if defined(DEBUG)
        if (assertStackDepth)
            emit<InstrAssertStackDepth>(stackDepth + delta);
#endif
    }

    void build(const Syntax& s, unsigned argCount) {
        assert(!block);
        assert(topLevel);
        assert(stackDepth == 0);
        assert(!parent || layout->slotCount() == argCount);
        bool hasNestedFuctions;
        defs = FindDefinitions(s, layout, globals, hasNestedFuctions);
        useLexicalEnv = useLexicalEnv || hasNestedFuctions;
        if (!parent)
            topLevel->extend(layout);
        layout = defs->layout();
        assert(layout->slotCount() >= argCount);
        block = gc.create<Block>(layout, argCount, useLexicalEnv);
        if (parent) {
            if (useLexicalEnv) {
                emit<InstrCreateEnv>();
                stackDepth = argCount;
            } else {
                emit<InstrInitStackLocals>();
                stackDepth = layout->slotCount();
            }
        }
        if (isGenerator) {
            emit<InstrStartGenerator>();
            emit<InstrPop>();
            stackDepth++;  // InstrStartGenerator leaves iterator on stack
        }
#ifdef DEBUG
        unsigned initialStackDepth = stackDepth;
#endif
        compile(s);
        assert(stackDepth == initialStackDepth);
    }

    void callUnaryMethod(const UnarySyntax& s, string name) {
        compile(s.right);
        emit<InstrGetMethod>(name);
        emit<InstrCallMethod>(0);
    }

    template <typename BaseType>
    void callBinaryMethod(const BinarySyntax<BaseType, Syntax, Syntax>& s,
                          string name)
    {
        compile(s.left);
        emit<InstrGetMethod>(name);
        compile(s.right);
        emit<InstrCallMethod>(1);
    }

    virtual void visit(const SyntaxPass& s) {
        emit<InstrConst>(None);
    }

    virtual void visit(const SyntaxGlobal& s) {
        emit<InstrConst>(None);
    }

    virtual void visit(const SyntaxNonLocal& s) {
        emit<InstrConst>(None);
    }

    virtual void visit(const SyntaxBlock& s) {
        maybeAssertStackDepth();
        bool first = true;
        for (const auto& i : s.statements) {
            if (!first)
                emit<InstrPop>();
            compile(i);
            maybeAssertStackDepth(1);
            first = false;
        }
        if (first)
            emit<InstrConst>(None);
    }

    virtual void visit(const SyntaxInteger& s) {
        Stack<Value> v(Integer::get(s.value));
        emit<InstrConst>(v);
    }

    virtual void visit(const SyntaxFloat& s) {
        Stack<Value> v(Float::get(s.value));
        emit<InstrConst>(v);
    }

    virtual void visit(const SyntaxString& s) {
        Stack<Value> v(String::get(s.value));
        emit<InstrConst>(v);
    }

    virtual void visit(const SyntaxExprList& s) {
        for (const auto& i : s.elements)
            compile(*i);
        emit<InstrTuple>(s.elements.size());
    }

    virtual void visit(const SyntaxList& s) {
        for (const auto& i : s.elements)
            compile(*i);
        emit<InstrList>(s.elements.size());
    }

    virtual void visit(const SyntaxDict& s) {
        for (const auto& i : s.entries) {
            compile(i.first);
            compile(i.second);
        }
        emit<InstrDict>(s.entries.size());
    }

    virtual void visit(const SyntaxOr& s) {
        compile(s.left);
        unsigned branch = emit<InstrOr>();
        compile(s.right);
        block->branchHere(branch);
    }

    virtual void visit(const SyntaxAnd& s) {
        compile(s.left);
        unsigned branch = emit<InstrAnd>();
        compile(s.right);
        block->branchHere(branch);
    }

    virtual void visit(const SyntaxNot& s) {
        compile(s.right);
        emit<InstrNot>();
    }

    virtual void visit(const SyntaxPos& s) {
        callUnaryMethod(s, Name::__pos__);
    }

    virtual void visit(const SyntaxNeg& s) {
        callUnaryMethod(s, Name::__neg__);
    }

    virtual void visit(const SyntaxInvert& s) {
        callUnaryMethod(s, Name::__invert__);
    }

    virtual void visit(const SyntaxBinaryOp& s) {
        compile(s.left);
        compile(s.right);
        emit<InstrBinaryOp>(s.op);
    }

    virtual void visit(const SyntaxAugAssign& s) {
        // todo: we'll need to change this if we want to specialise depending on
        // whether the target implements update-in-place methods
        compile(s.left);
        compile(s.right);
        emit<InstrAugAssignUpdate>(s.op);
        {
            AutoPushContext enterAssign(contextStack, Context::Assign);
            compile(s.left);
        }
        emit<InstrPop>();
        emit<InstrConst>(None);
    }

    virtual void visit(const SyntaxCompareOp& s) {
        // todo: == should fall back to comparing identity if __eq__ not
        // implemented
        compile(s.left);
        compile(s.right);
        emit<InstrCompareOp>(s.op);
    }


#define define_vist_binary_instr(syntax, instr)                               \
    virtual void visit(const syntax& s) {                                     \
        compile(s.left);                                                      \
        compile(s.right);                                                     \
        emit<instr>();                                                        \
    }

    define_vist_binary_instr(SyntaxIn, InstrIn);
    define_vist_binary_instr(SyntaxIs, InstrIs);

#undef define_vist_binary_instr

    virtual void visit(const SyntaxAssign& s) {
        compile(s.right);
        AutoPushContext enterAssign(contextStack, Context::Assign);
        compile(s.left);
    }

    virtual void visit(const SyntaxDel& s) {
        AutoPushContext enterDelete(contextStack, Context::Delete);
        compile(s.targets);
    }

    bool lookupLocal(Name name, int& slotOut) {
        if (!parent || useLexicalEnv)
            return false;
        slotOut = layout->lookupName(name);
        return slotOut != Layout::NotFound;
    }

    bool lookupLexical(Name name, unsigned& frameOut) {
        if (contains(globals, name))
            return false;

        int frame = 0;
        ByteCompiler* bc = useLexicalEnv ? this : parent;
        while (bc && bc->parent) {
            if (bc->layout->hasName(name)) {
                frameOut = frame;
                return true;
            }
            if (bc->useLexicalEnv)
                ++frame;
            bc = bc->parent;
        }
        return false;
    }

    bool lookupGlobal(Name name) {
        return
            (!parent && layout->lookupName(name) != Layout::NotFound) ||
            topLevel->hasAttr(name) ||
            contains(globals, name);
    }

    virtual void visit(const SyntaxName& s) {
        Name name = s.id;
        int slot;
        unsigned frame;
        switch(contextStack.back()) {
          case Context::Assign:
            if (lookupLocal(name, slot))
                emit<InstrSetStackLocal>(name, slot);
            else if (lookupLexical(name, frame))
                emit<InstrSetLexical>(frame, name);
            else if (lookupGlobal(name))
                emit<InstrSetGlobal>(topLevel, name);
            else
                throw ParseError(s.token,
                                 string("Name is not defined: ") + name);
            break;

          case Context::Delete:
            if (lookupLocal(name, slot))
                emit<InstrDelStackLocal>(name, slot);
            else if (lookupLexical(name, frame))
                emit<InstrDelLexical>(frame, name);
            else if (lookupGlobal(name))
                emit<InstrDelGlobal>(topLevel, name);
            else
                throw ParseError(s.token,
                                 string("Name is not defined: ") + name);
            emit<InstrConst>(None);
            break;

          default:
            if (lookupLocal(name, slot))
                emit<InstrGetStackLocal>(name, slot);
            else if (lookupLexical(name, frame))
                emit<InstrGetLexical>(frame, name);
            else
                emit<InstrGetGlobal>(topLevel, name);
            break;
        }
    }

    virtual void visit(const SyntaxAttrRef& s) {
        {
            AutoPushContext clearContext(contextStack, Context::None);
            compile(s.left);
        }
        Name id = s.right->id;

        switch(contextStack.back()) {
          case Context::Assign:
            emit<InstrSetAttr>(id);
            break;

          case Context::Delete:
            emit<InstrDelAttr>(id);
            emit<InstrConst>(None);
            break;

          default:
            emit<InstrGetAttr>(id);
            break;
        }
    }

    virtual void visit(const SyntaxSubscript& s) {
        switch(contextStack.back()) {
          case Context::Assign: {
            AutoPushContext clearContext(contextStack, Context::None);
            compile(s.left);
            emit<InstrGetMethod>(Name::__setitem__);
            compile(s.right);
            // todo: there may be a better way than this stack manipulation
            emit<InstrDup>(4);
            emit<InstrCallMethod>(2);
            emit<InstrSwap>();
            emit<InstrPop>();
            break;
          }

          case Context::Delete: {
            AutoPushContext clearContext(contextStack, Context::None);
            callBinaryMethod(s, Name::__delitem__);
            break;
          }

          default:
              callBinaryMethod(s, Name::__getitem__);
            break;
        }
    }

    virtual void visit(const SyntaxTargetList& s) {
        // todo: we can probably optimise this depending on the syntax used.
        // e.g. a, b = 1, 2 can be desugared into a = 1; b = 2
        assert(inTarget());
        const auto& targets = s.targets;
        if (contextStack.back() == Context::Assign) {
            // Get length
            emit<InstrDup>();
            emit<InstrGetMethod>(Name::__len__);
            emit<InstrCallMethod>(0);

            // Check length
            Stack<Value> size(Integer::get(targets.size()));
            emit<InstrConst>(size);
            emit<InstrCompareOp>(CompareEQ);
            unsigned okBranch = emit<InstrBranchIfTrue>();
            emit<InstrConst>(ValueError::ObjectClass);
            Stack<Value> message(
                String::get("wrong number of values to unpack"));
            emit<InstrConst>(message);
            emit<InstrCall>(1);
            emit<InstrRaise>();
            block->branchHere(okBranch);

            // Get iterator
            emit<InstrGetMethod>(Name::__iter__);
            emit<InstrCallMethod>(0);
            emit<InstrGetMethod>("next");
            stackDepth += 3; // Leave the iterator and next method on the stack

            for (unsigned i = 0; i < targets.size(); i++) {
                emit<InstrIteratorNext>();
                unsigned okBranch = emit<InstrBranchIfTrue>();
                emit<InstrConst>(None);
                emit<InstrAssertionFailed>();
                block->branchHere(okBranch);
                compile(targets[i]);
                if (i != targets.size() - 1)
                    emit<InstrPop>();
            }

            // Pop iterator
            emit<InstrPop>();
            emit<InstrPop>();
            emit<InstrPop>();
            stackDepth -= 3;
        } else {
            for (unsigned i = 0; i < targets.size(); i++) {
                compile(targets[i]);
                if (i != targets.size() - 1)
                    emit<InstrPop>();
            }
        }

    }

    virtual void visit(const SyntaxSlice& s) {
        if (s.lower)
            compile(s.lower);
        else
            emit<InstrConst>(None);

        if (s.upper)
            compile(s.upper);
        else
            emit<InstrConst>(None);

        if (s.stride)
            compile(s.stride);
        else
            emit<InstrConst>(None);

        emit<InstrSlice>();
    }

    virtual void visit(const SyntaxCall& s) {
        // todo: check this actually how python works
        bool methodCall = s.left->is<SyntaxAttrRef>();

        if (methodCall) {
            const SyntaxAttrRef* pr = s.left->as<SyntaxAttrRef>();
            compile(pr->left);
            emit<InstrGetMethod>(pr->right->id);
        } else {
            compile(s.left);
        }

        for (const auto& i : s.right)
            compile(*i);
        unsigned count = s.right.size();
        if (methodCall)
            emit<InstrCallMethod>(count);
        else
            emit<InstrCall>(count);
    }

    virtual void visit(const SyntaxReturn& s) {
        if (isClassBlock) {
            throw ParseError(s.token,
                             "Return statement not allowed in class body");
        } else if (isGenerator) {
            if (s.right) {
                throw ParseError(s.token,
                                 "SyntaxError: 'return' with argument inside generator");
            }
            emit<InstrLeaveGenerator>();
        } else {
            if (s.right)
                compile(s.right);
            else
                emit<InstrConst>(None);
            emit<InstrReturn>();
        }
    }

    virtual void visit(const SyntaxCond& s) {
        compile(s.cond);
        unsigned altBranch = emit<InstrBranchIfFalse>();
        compile(s.cons);
        unsigned endBranch = emit<InstrBranchAlways>();
        block->branchHere(altBranch);
        compile(s.alt);
        block->branchHere(endBranch);
    }

    virtual void visit(const SyntaxIf& s) {
        const auto& suites = s.branches;
        assert(suites.size() != 0);
        vector<unsigned> branchesToEnd;
        unsigned lastCondFailed = 0;
        for (unsigned i = 0; i != suites.size(); ++i) {
            if (i != 0)
                block->branchHere(lastCondFailed);
            compile(suites[i].cond);
            lastCondFailed = emit<InstrBranchIfFalse>();
            compile(suites[i].suite);
            emit<InstrPop>();
            if (s.elseSuite || i != suites.size() - 1)
                branchesToEnd.push_back(emit<InstrBranchAlways>());
        }
        block->branchHere(lastCondFailed);
        if (s.elseSuite) {
            compile(s.elseSuite);
            emit<InstrPop>();
        }
        for (unsigned i = 0; i < branchesToEnd.size(); ++i)
            block->branchHere(branchesToEnd[i]);
        emit<InstrConst>(None);
    }

    virtual void visit(const SyntaxWhile& s) {
        AutoSetAndRestoreOffset setLoopHead(loopHeadOffset, block->nextIndex());
        vector<unsigned> oldBreakInstrs = move(breakInstrs);
        breakInstrs.clear();
        compile(s.cond);
        unsigned branchToEnd = emit<InstrBranchIfFalse>();
        {
            AutoPushContext enterLoop(contextStack, Context::Loop);
            compile(s.suite);
            emit<InstrPop>();
        }
        emit<InstrBranchAlways>(block->offsetTo(loopHeadOffset));
        block->branchHere(branchToEnd);
        setBreakTargets();
        breakInstrs = move(oldBreakInstrs);
        emit<InstrConst>(None);
        // todo: else
    }

    virtual void visit(const SyntaxFor& s) {
        // 1. Get iterator
        compile(s.exprs);
        emit<InstrGetMethod>(Name::__iter__);
        emit<InstrCallMethod>(0);
        emit<InstrGetMethod>("next");
        stackDepth += 3; // Leave the iterator and next method on the stack

        // 2. Call next on iterator and break if end (loop heap)
        AutoSetAndRestoreOffset setLoopHead(loopHeadOffset, block->nextIndex());
        vector<unsigned> oldBreakInstrs = move(breakInstrs);
        breakInstrs.clear();
        // todo: can we use AutoSetAndRestore for the above?

        emit<InstrIteratorNext>();
        unsigned exitBranch = emit<InstrBranchIfFalse>();

        // 3. Assign results
        {
            AutoPushContext enterAssign(contextStack, Context::Assign);
            compile(s.targets);
            emit<InstrPop>();
        }

        // 4. Execute loop body
        {
            AutoPushContext enterLoop(contextStack, Context::Loop);
            compile(s.suite);
            emit<InstrPop>();
        }
        emit<InstrBranchAlways>(block->offsetTo(loopHeadOffset));

        // 5. Exit
        block->branchHere(exitBranch);

        // 6. Else clause
        if (s.elseSuite) {
            compile(s.elseSuite);
            emit<InstrPop>();
        }

        setBreakTargets();
        breakInstrs = move(oldBreakInstrs);
        emit<InstrPop>();
        emit<InstrPop>();
        emit<InstrPop>();
        stackDepth -= 3;
        emit<InstrConst>(None);
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
        Stack<Block*> exprBlock(
            ByteCompiler().buildLambda(this, s.params, *s.expr));
        emitLambda("(lambda)", s.params, exprBlock, false);
    }

    virtual void visit(const SyntaxDef& s) {
        Stack<Block*> exprBlock;
        ByteCompiler compiler;
        if (s.isGenerator)
            exprBlock = compiler.buildGenerator(this, s.params, *s.suite);
        else
            exprBlock = compiler.buildFunctionBody(this, s.params, *s.suite);
        emitLambda(s.id, s.params, exprBlock, s.isGenerator);
        int slot;
        unsigned frame;
        if (lookupLocal(s.id, slot))
            emit<InstrSetStackLocal>(s.id, slot);
        else if (lookupLexical(s.id, frame))
            emit<InstrSetLexical>(frame, s.id);
        else
            emit<InstrSetGlobal>(topLevel, s.id);
    }

    virtual void visit(const SyntaxClass& s) {
        Stack<Block*> suite(ByteCompiler().buildClass(this, s.id, *s.suite));
        vector<Name> params = { Name::__bases__ };
        emit<InstrLambda>(s.id, params, suite);
        compile(s.bases);
        emit<InstrCall>(1);
        if (parent) {
            int slot;
            alwaysTrue(lookupLocal(s.id, slot));
            emit<InstrSetStackLocal>(s.id, slot);
        } else {
            emit<InstrSetGlobal>(topLevel, s.id);
        }
    }

    virtual void visit(const SyntaxAssert& s) {
        if (debugMode) {
            compile(s.cond);
            unsigned endBranch = emit<InstrBranchIfTrue>();
            if (s.message) {
                compile(s.message);
                emit<InstrGetMethod>(Name::__str__);
                emit<InstrCallMethod>(0);
            } else {
                emit<InstrConst>(None);
            }
            emit<InstrAssertionFailed>();
            block->branchHere(endBranch);
        }
        emit<InstrConst>(None);
    }

    virtual void visit(const SyntaxRaise& s) {
        compile(s.right);
        emit<InstrRaise>();
    }

    virtual void visit(const SyntaxYield& s) {
        compile(s.right);
        emit<InstrSuspendGenerator>();
    }

    virtual void visit(const SyntaxTry& s) {
        unsigned finallyBranch = 0;
        if (s.finallySuite) {
            contextStack.push_back(Context::Finally);
            finallyBranch = emit<InstrEnterFinallyRegion>();
        }
        if (s.excepts.size() != 0) {
            emitTryCatch(s);
        } else {
            compile(s.trySuite);
            emit<InstrPop>();
        }
        if (s.finallySuite) {
            emit<InstrLeaveFinallyRegion>();
            block->branchHere(finallyBranch);
            compile(s.finallySuite);
            emit<InstrPop>();
            emit<InstrFinishExceptionHandler>();
            assert(contextStack.back() == Context::Finally);
            contextStack.pop_back();
        }
        emit<InstrConst>(None);
    }

    virtual void emitTryCatch(const SyntaxTry& s) {
        unsigned handlerBranch = emit<InstrEnterCatchRegion>();
        compile(s.trySuite);
        emit<InstrPop>();
        emit<InstrLeaveCatchRegion>();
        unsigned suiteEndBranch = emit<InstrBranchAlways>();

        bool fullyHandled = false;
        vector<unsigned> exceptEndBranches;
        for (const auto& e : s.excepts) {
            assert(!fullyHandled);
            block->branchHere(handlerBranch);
            if (e->expr) {
                compile(e->expr);
                emit<InstrMatchCurrentException>();
                handlerBranch = emit<InstrBranchIfFalse>();
                if (e->as) {
                    AutoPushContext enterAssign(contextStack, Context::Assign);
                    compile(e->as);
                }
                emit<InstrPop>();
            } else {
                assert(!e->as);
                fullyHandled = true;
                emit<InstrHandleCurrentException>();
            }
            compile(e->suite);
            emit<InstrPop>();
            exceptEndBranches.push_back(emit<InstrBranchAlways>());
        }

        block->branchHere(suiteEndBranch);
        if (s.elseSuite) {
            compile(s.elseSuite);
            emit<InstrPop>();
        }

        if (!fullyHandled)
            block->branchHere(handlerBranch);
        emit<InstrFinishExceptionHandler>();
        for (unsigned offset: exceptEndBranches)
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
            throw ParseError(s.token, "SyntaxError: 'break' outside loop");
        unsigned finallyCount = countFinallyBlocksInLoop();
        unsigned offset = emit<InstrLoopControlJump>(finallyCount);
        breakInstrs.push_back(offset);
    }

    virtual void visit(const SyntaxContinue& s) {
        if (!inLoop())
            throw ParseError(s.token, "SyntaxError: 'continue' outside loop");
        unsigned finallyCount = countFinallyBlocksInLoop();
        emit<InstrLoopControlJump>(finallyCount, loopHeadOffset);
    }

    virtual void visit(const SyntaxImport& s) {
        // todo: implement import ...
        emit<InstrConst>(None);
    }

    virtual void visit(const SyntaxFrom& s) {
        // todo: implement from ... import ...
        emit<InstrConst>(None);
    }

    virtual void setPos(const TokenPos& pos) {
        block->setNextPos(pos);
    }
};

void CompileModule(const Input& input, Traced<Object*> globalsArg,
                   MutableTraced<Block*> blockOut)
{
    SyntaxParser parser;
    parser.start(input);
    unique_ptr<SyntaxBlock> syntax(parser.parseModule());
    Stack<Object*> globals(globalsArg);
    if (globals->isNone())
        globals = createTopLevel();
    blockOut = ByteCompiler().buildModule(globals, syntax.get());
}
