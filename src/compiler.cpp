#include "analysis.h"
#include "builtin.h"
#include "common.h"
#include "compiler.h"
#include "exception.h"
#include "frame.h"
#include "instr.h"
#include "parser.h"
#include "repr.h"
#include "reflect.h"
#include "singletons.h"
#include "utils.h"

#ifdef DEBUG

bool assertStackDepth = true;
bool logCompile = false;

static inline void log(const Block* block) {
    if (logCompile)
        cout << "bc: " << repr(*block) << endl;
}

static inline void log(const char* message) {
    if (logCompile)
        cout << "bc: " << message << endl;
}

static inline void logBuild(unsigned argCount) {
    if (logCompile)
        cout << "bc:   build " << dec << argCount << endl;
}

static inline void logEmit(unsigned index, Instr* instr, int stackDepth)
{
    if (logCompile) {
        cout << "bc:   emit " << dec << index << " " << *instr;
        cout << " " << stackDepth << endl;
    }
}

static inline void logBranchHere(unsigned index, int stackDepth,
                                 int sourceStackDepth)
{
    if (logCompile) {
        cout << "bc:   branchHereFrom " << dec << index << " " << stackDepth;
        cout << " " << sourceStackDepth << endl;
    }
}

#else

static inline void log(const Block* block) {}
static inline void log(const char* message) {}
static inline void logBuild(unsigned argCount) {}
static inline void logEmit(unsigned index, Instr* instr, int stackDepth) {}
static inline void logBranchHere(unsigned index, int stackDepth,
                                 int sourceStackDepth) {}

#endif

struct ByteCompiler : public SyntaxVisitor
{
    ByteCompiler()
      : parent(nullptr),
        topLevel(nullptr),
        layout(Env::InitialLayout),
        useLexicalEnv(false),
        block(nullptr),
        contextStack({Context::None}),
        loopHeadOffset(0),
        stackDepth(0),
        maxStackDepth(0)
    {}

    ~ByteCompiler() {
        assert(contextStack.size() == 1);
        assert(contextStack.back() == Context::None);
        assert(loopHeadOffset == 0);
        assert(breakInstrs.empty());
    }

    Block* buildModule(Traced<Env*> globals, const SyntaxBlock* s) {
        log("buildModule");
        assert(globals);
        assert(!parent);
        kind = Kind::Module;
        topLevel = globals;
        layout = topLevel->layout();
        build(*s, 0);
        emit<Instr_Return>();
        log(block);
        return block;
    }

    Block* buildFunctionBody(ByteCompiler* parent,
                             const vector<Parameter>& params,
                             const Syntax& s)
    {
        log("buildFunctionBody");
        assert(parent);
        kind = Kind::FunctionBody;
        this->parent = parent->block;
        topLevel = parent->topLevel;
        for (auto i = params.begin(); i != params.end(); ++i)
            layout = layout->addName(i->name);
        // Environment and parameters passed on stack
        build(s, params.size());
        if (block->lastInstr().data->code() != Instr_Return) {
            emit<Instr_Pop>();
            emit<Instr_Const>(None);
            emit<Instr_Return>();
        }
        log(block);
        return block;
    }

    Block* buildLambda(ByteCompiler* parent, const vector<Parameter>& params,
                       const Syntax& s)
    {
        log("buildLambda");
        assert(parent);
        kind = Kind::Lambda;
        this->parent = parent->block;
        topLevel = parent->topLevel;
        for (auto i = params.begin(); i != params.end(); ++i)
            layout = layout->addName(i->name);
        build(s, params.size());
        emit<Instr_Return>();
        log(block);
        return block;
    }

    Block* buildClass(ByteCompiler* parent, Name id, const Syntax& s) {
        log("buildClass");
        assert(parent);
        kind = Kind::ClassBlock;
        this->parent = parent->block;
        topLevel = parent->topLevel;
        layout = layout->addName(Names::__bases__);
        useLexicalEnv = true;
        build(s, 1);
        emit<Instr_Pop>();
        emit<Instr_MakeClassFromFrame>(id);
        emit<Instr_Return>();
        log(block);
        return block;
    }

    Block* buildGenerator(ByteCompiler* parent,
                          const vector<Parameter>& params,
                          const Syntax& s)
    {
        log("buildGenerator");
        assert(parent);
        kind = Kind::Generator;
        this->parent = parent->block;
        topLevel = parent->topLevel;
        for (auto i = params.begin(); i != params.end(); ++i)
            layout = layout->addName(i->name);
        build(s, params.size());
        if (block->lastInstr().data->code() != Instr_LeaveGenerator) {
            emit<Instr_Pop>();
            emit<Instr_LeaveGenerator>();
        }
        log(block);
        return block;
    }

    Block* buildListComp(ByteCompiler* parent, const SyntaxListComp& s) {
        log("buildListComp");
        assert(parent);
        kind = Kind::ListComp;
        this->parent = parent->block;
        topLevel = parent->topLevel;
        layout = layout->addName(Names::listCompResult);
        build(*s.expr, 0);
        emit<Instr_Return>();
        log(block);
        return block;
    }

    Block* buildEval(Traced<Env*> globals, Traced<Env*> locals,
                     const Syntax* s)
    {
        log("buildEval");
        assert(globals && locals);

        // todo: this is a hack to create a setup where we have the globals in a
        // separate environements both above the code being compiled
        Stack<Layout*> blockLayout(globals->layout());
        parent = gc.create<Block>(nullptr, globals, blockLayout, 0, true);

        useLexicalEnv = true;
        layout = locals->layout();
        kind = Kind::Eval;
        topLevel = globals;

        build(*s, 0, locals);
        emit<Instr_Return>();
        log(block);
        return block;
    }

    Block* buildExec(Traced<Env*> globals, Traced<Env*> locals,
                     const Syntax* s)
    {
        log("buildExec");
        assert(globals && locals);

        // todo: this is a hack to create a setup where we have the globals in a
        // separate environements both above the code being compiled
        Stack<Layout*> blockLayout(globals->layout());
        parent = gc.create<Block>(nullptr, globals, blockLayout, 0, true);

        useLexicalEnv = true;
        layout = locals->layout();
        kind = Kind::Exec;
        topLevel = globals;

        build(*s, 0, locals);
        emit<Instr_Pop>();
        emit<Instr_Const>(None);
        emit<Instr_Return>();
        log(block);
        return block;
    }

  private:
    enum class Kind {
        Module,
        FunctionBody,
        Lambda,
        ClassBlock,
        Generator,
        ListComp,
        Exec,
        Eval
    };

    enum class Context {
        None,
        Loop,
        Finally,
        Assign,
        Delete
    };

    Kind kind;
    Root<Block*> parent;
    Root<Env*> topLevel;
    Root<Layout*> layout;
    Root<Object*> defs;
    vector<Name> globals;
    bool useLexicalEnv;
    Root<Block*> block;
    vector<Context> contextStack;
    unsigned loopHeadOffset;
    vector<unsigned> breakInstrs;
    int stackDepth; // -1 => unknown, e.g. after BranchAlways
    int maxStackDepth;
    vector<unsigned> stackDepthAtInstr;

    using AutoPushContext = AutoPushStack<Context>;
    using AutoSetAndRestoreOffset = AutoSetAndRestoreValue<unsigned>;

    bool inLoop() {
        return contains(contextStack, Context::Loop);
    }

    bool inTarget() {
        return contextStack.back() == Context::Assign ||
               contextStack.back() == Context::Delete;
    }

    template <InstrCode Type, typename... Args>
    unsigned emit(Args&& ...args) {
        using Factory = InstrFactory<Type>;
        using Class = typename Factory::Class;
        Stack<Class*> instr(Factory::get(forward<Args>(args)...));
        unsigned index = block->append(instr);

        if (stackDepth != -1) {
            stackDepth += InstrStackAdjustment<Type>::value;
            stackDepth += InstrStackAdjustmentCountMultiple<Type>::Get(instr);
        }

        assert(stackDepthAtInstr.size() == index);
        stackDepthAtInstr.push_back(stackDepth);
        maxStackDepth = max(stackDepth, maxStackDepth);

        logEmit(index, instr, stackDepth);

        if (InstrIsUnconditionalBranch<Type>::value)
            stackDepth = -1;
        return index;
    }

    void branchHereFrom(unsigned index) {
        assert(block->instrCount() != 0);
        assert(index < block->instrCount());
        logBranchHere(index, stackDepth, stackDepthAtInstr[index]);
        if (stackDepth == -1)
            stackDepth = stackDepthAtInstr[index];
        else
            assert(stackDepth == stackDepthAtInstr[index]);
        block->branchHere(index);
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
            Instr *instr = block->instr(source).data;
            assert(instr->code() == Instr_LoopControlJump);
            static_cast<LoopControlJumpInstr*>(instr)->setTarget(pos);
            // todo: check stack depth
        }
        breakInstrs.clear();
    }

    void maybeAssertStackDepth() {
#if defined(DEBUG)
        if (assertStackDepth && stackDepth != -1)
            emit<Instr_AssertStackDepth>(stackDepth);
#endif
    }

    void build(const Syntax& s, unsigned argCount,
               Traced<Env*> localEnv = nullptr)
    {
        logBuild(argCount);
        assert(!block);
        assert(topLevel);
        assert(stackDepth == 0);
        assert(!parent || kind == Kind::ListComp ||
               kind == Kind::Eval || kind == Kind::Exec ||
               layout->slotCount() == argCount);
        bool hasNestedFuctions;
        defs = FindDefinitions(s, layout, globals, hasNestedFuctions);
        useLexicalEnv = useLexicalEnv || hasNestedFuctions;
        if (kind == Kind::Module) {
            assert(!parent);
            topLevel->extend(layout);
        }
        layout = defs->layout();
        assert(layout->slotCount() >= argCount);
        block = gc.create<Block>(parent, topLevel, layout, argCount,
                                 useLexicalEnv);

        stackDepth = (kind != Kind::Module ? 1 : 0) + argCount;
        maybeAssertStackDepth();

        if (parent) {
            if (useLexicalEnv) {
                if (localEnv)
                    emit<Instr_SetEnv>(localEnv);
                else
                    emit<Instr_CreateEnv>();
            } else {
                emit<Instr_InitStackLocals>(block->stackLocalCount());
            }
            maybeAssertStackDepth();
        }

        if (kind == Kind::Generator) {
            emit<Instr_StartGenerator>();
            emit<Instr_Pop>();
        }
#ifdef DEBUG
        unsigned initialStackDepth = stackDepth;
#endif
        if (kind == Kind::ListComp)
            compileListComp(s);
        else
            compile(s);

        assert(stackDepth == initialStackDepth + 1);
        block->setMaxStackDepth(maxStackDepth + 2);
    }

    void callUnaryMethod(const UnarySyntax& s, Name name) {
        compile(s.right);
        emit<Instr_GetMethod>(name);
        emit<Instr_CallMethod>(0);
    }

    template <typename BaseType>
    void callBinaryMethod(const BinarySyntax<BaseType, Syntax, Syntax>& s,
                          Name name)
    {
        compile(s.left);
        emit<Instr_GetMethod>(name);
        compile(s.right);
        emit<Instr_CallMethod>(1);
    }

    virtual void visit(const SyntaxPass& s) {
        emit<Instr_Const>(None);
    }

    virtual void visit(const SyntaxGlobal& s) {
        emit<Instr_Const>(None);
    }

    virtual void visit(const SyntaxNonLocal& s) {
        emit<Instr_Const>(None);
    }

    virtual void visit(const SyntaxBlock& s) {
        maybeAssertStackDepth();
        bool first = true;
        for (const auto& i : s.statements) {
            if (!first)
                emit<Instr_Pop>();
            compile(i);
            maybeAssertStackDepth();
            first = false;
        }
        if (first)
            emit<Instr_Const>(None);
    }

    virtual void visit(const SyntaxInteger& s) {
        Stack<Value> v(Integer::get(s.token.text));
        emit<Instr_Const>(v);
    }

    virtual void visit(const SyntaxFloat& s) {
        Stack<Value> v(Float::get(s.value));
        emit<Instr_Const>(v);
    }

    virtual void visit(const SyntaxString& s) {
        Stack<Value> v(String::get(s.value));
        emit<Instr_Const>(v);
    }

    virtual void visit(const SyntaxExprList& s) {
        for (const auto& i : s.elements)
            compile(*i);
        emit<Instr_Tuple>(s.elements.size());
    }

    virtual void visit(const SyntaxList& s) {
        for (const auto& i : s.elements)
            compile(*i);
        emit<Instr_List>(s.elements.size());
    }

    virtual void visit(const SyntaxDict& s) {
        for (const auto& i : s.entries) {
            compile(i.first);
            compile(i.second);
        }
        emit<Instr_Dict>(s.entries.size());
    }

    virtual void visit(const SyntaxOr& s) {
        compile(s.left);
        unsigned branch = emit<Instr_Or>();
        emit<Instr_Pop>();
        compile(s.right);
        branchHereFrom(branch);
    }

    virtual void visit(const SyntaxAnd& s) {
        compile(s.left);
        unsigned branch = emit<Instr_And>();
        emit<Instr_Pop>();
        compile(s.right);
        branchHereFrom(branch);
    }

    virtual void visit(const SyntaxNot& s) {
        compile(s.right);
        emit<Instr_Not>();
    }

    virtual void visit(const SyntaxPos& s) {
        callUnaryMethod(s, Names::__pos__);
    }

    virtual void visit(const SyntaxNeg& s) {
        callUnaryMethod(s, Names::__neg__);
    }

    virtual void visit(const SyntaxInvert& s) {
        callUnaryMethod(s, Names::__invert__);
    }

    virtual void visit(const SyntaxBinaryOp& s) {
        compile(s.left);
        compile(s.right);
        emit<Instr_BinaryOp>(s.op);
    }

    virtual void visit(const SyntaxAugAssign& s) {
        compile(s.left);
        compile(s.right);
        emit<Instr_AugAssignUpdate>(s.op);
        {
            AutoPushContext enterAssign(contextStack, Context::Assign);
            compile(s.left);
        }
        emit<Instr_Pop>();
        emit<Instr_Const>(None);
    }

    virtual void visit(const SyntaxCompareOp& s) {
        compile(s.left);
        compile(s.right);
        emit<Instr_CompareOp>(s.op);
    }


#define define_visit_binary_instr(syntax, instr)                              \
    virtual void visit(const syntax& s) {                                     \
        compile(s.left);                                                      \
        compile(s.right);                                                     \
        emit<instr>();                                                        \
    }

    define_visit_binary_instr(SyntaxIn, Instr_In);
    define_visit_binary_instr(SyntaxIs, Instr_Is);

#undef define_visit_binary_instr

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
        // todo: this looks wrong
        if (contains(globals, name))
            return false;

        int frame = 0;
        Stack<Block*> block(useLexicalEnv ? this->block : parent);
        while (block && block->parent()) {
            if (block->layout()->hasName(name)) {
                frameOut = frame;
                return true;
            }
            ++frame;
            block = block->parent();
        }
        return false;
    }

    bool lookupGlobal(Name name) {
        return
            (!parent && layout->lookupName(name) != Layout::NotFound) ||
            topLevel->hasAttr(name) ||
            contains(globals, name);
    }

    void compileAssign(Name name) {
        int slot;
        unsigned frame;
        if (lookupLocal(name, slot)) {
            emit<Instr_SetStackLocal>(name, slot);
        } else if (lookupLexical(name, frame)) {
            emit<Instr_SetLexical>(frame, name);
        } else {
            assert(lookupGlobal(name));
            emit<Instr_SetGlobal>(topLevel, name);
        }
    }

    void compileDelete(Name name) {
        int slot;
        unsigned frame;
        if (lookupLocal(name, slot)) {
            emit<Instr_DelStackLocal>(name, slot);
        } else if (lookupLexical(name, frame)) {
            emit<Instr_DelLexical>(frame, name);
        } else {
            assert(lookupGlobal(name));
            emit<Instr_DelGlobal>(topLevel, name);
        }
        emit<Instr_Const>(None);
    }

    void compileReference(Name name) {
        int slot;
        unsigned frame;
        if (lookupLocal(name, slot))
            emit<Instr_GetStackLocal>(name, slot);
        else if (lookupLexical(name, frame))
            emit<Instr_GetLexical>(frame, name);
        else
            emit<Instr_GetGlobal>(topLevel, name);
    }

    virtual void visit(const SyntaxName& s) {
        switch(contextStack.back()) {
          case Context::Assign:
            compileAssign(s.id);
            break;

          case Context::Delete:
            compileDelete(s.id);
            break;

          default:
            compileReference(s.id);
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
            emit<Instr_SetAttr>(id);
            break;

          case Context::Delete:
            emit<Instr_DelAttr>(id);
            emit<Instr_Const>(None);
            break;

          default:
            emit<Instr_GetAttr>(id);
            break;
        }
    }

    virtual void visit(const SyntaxSubscript& s) {
        switch(contextStack.back()) {
          case Context::Assign: {
            AutoPushContext clearContext(contextStack, Context::None);
            compile(s.left);
            emit<Instr_GetMethod>(Names::__setitem__);
            compile(s.right);
            // todo: there may be a better way than this stack manipulation
            emit<Instr_Dup>(3);
            emit<Instr_CallMethod>(2);
            emit<Instr_Swap>();
            emit<Instr_Pop>();
            break;
          }

          case Context::Delete: {
            AutoPushContext clearContext(contextStack, Context::None);
            callBinaryMethod(s, Names::__delitem__);
            break;
          }

          default:
            callBinaryMethod(s, Names::__getitem__);
            break;
        }
    }

    virtual void visit(const SyntaxTargetList& s) {
        assert(inTarget());
        const auto& targets = s.targets;
        assert(contextStack.back() == Context::Assign ||
               contextStack.back() == Context::Delete);
        bool isAssign = contextStack.back() == Context::Assign;
        if (isAssign)
            emit<Instr_Destructure>(targets.size());
        // Reverse order here becuase destructure reverses the order when
        // pushing onto the stack.
        for (unsigned i = targets.size(); i != 0; i--) {
            compile(targets[i - 1]);
            emit<Instr_Pop>();
        }
        emit<Instr_Const>(None);
    }

    virtual void visit(const SyntaxSlice& s) {
        if (s.lower)
            compile(s.lower);
        else
            emit<Instr_Const>(None);

        if (s.upper)
            compile(s.upper);
        else
            emit<Instr_Const>(None);

        if (s.stride)
            compile(s.stride);
        else
            emit<Instr_Const>(None);

        emit<Instr_Slice>();
    }

    virtual void visit(const SyntaxCall& s) {
        int initialStackDepth = stackDepth;

        // todo: check this actually how python works
        bool methodCall = s.target->is<SyntaxAttrRef>();
        if (methodCall) {
            const SyntaxAttrRef* pr = s.target->as<SyntaxAttrRef>();
            compile(pr->left);
            emit<Instr_GetMethod>(pr->right->id);
        } else {
            compile(s.target);
        }

        int argsPos = stackDepth;
        size_t unpackedCount = 0;
        for (const auto& i : s.positionalArgs) {
            compile(*i->arg);
            if (i->isUnpacked) {
                // This pushes a variable number of values onto the stack, so
                // the stack depth is subseqently unknown.
                emit<Instr_UnpackArgs>();
                stackDepth = -1;
                unpackedCount++;
            }
        }
        for (const auto& i : s.keywordArgs)
            compile(*i->arg);
        if (s.mappingArg)
            compile(*s.mappingArg);

        if (s.mappingArg)
            throw ParseError(s.token, "**kwargs not implemented");

        size_t posCount = s.positionalArgs.size();
        size_t keywordCount = s.keywordArgs.size();
        if (keywordCount == 0 && unpackedCount == 0) {
            if (methodCall) {
                emit<Instr_CallMethod>(posCount);
            } else {
                // Calling a constructor needs an extra stack slot to pass the
                // class to __new__ and self to __init__.
                maxStackDepth = max(stackDepth + 1, maxStackDepth);
                emit<Instr_Call>(posCount);
            }
        } else {
            // Make a layout containing the keyword names in reverse order.
            Stack<Layout*> keywords(Layout::Empty);
            for (size_t i = s.keywordArgs.size(); i != 0; i--) {
                const auto& info = s.keywordArgs[i - 1];
                bool existing;
                keywords = keywords->maybeAddName(info->keyword->id, existing);
                if (existing)
                    throw ParseError(info->keyword->token,
                                     "Repeated keyword arg");
            }
            if (unpackedCount != 0)
                posCount = SIZE_MAX; // Unknown in this case.
            if (methodCall) {
                emit<Instr_CallMethodWithFullArgs>(argsPos, posCount, keywords);
            } else {
                // Calling a constructor needs an extra stack slot to pass the
                // class to __new__ and self to __init__.
                maxStackDepth = max(stackDepth + 1, maxStackDepth);
                emit<Instr_CallWithFullArgs>(argsPos, posCount, keywords);
            }
        }
        assert(stackDepth == -1 || stackDepth == initialStackDepth + 1);
        if (stackDepth == -1)
            stackDepth = initialStackDepth + 1;
    }

    virtual void visit(const SyntaxReturn& s) {
        if (kind == Kind::ClassBlock) {
            throw ParseError(s.token,
                             "Return statement not allowed in class body");
        } else if (kind == Kind::Generator) {
            if (s.right) {
                throw ParseError(s.token,
                                 "SyntaxError: 'return' with argument inside generator");
            }
            emit<Instr_LeaveGenerator>();
        } else if (parent) {
            if (s.right)
                compile(s.right);
            else
                emit<Instr_Const>(None);
            emit<Instr_Return>();
        } else {
            throw ParseError(s.token, "SyntaxError: 'return' outside function");
        }
    }

    virtual void visit(const SyntaxCond& s) {
        compile(s.cond);
        unsigned altBranch = emit<Instr_BranchIfFalse>();
        compile(s.cons);
        unsigned endBranch = emit<Instr_BranchAlways>();
        branchHereFrom(altBranch);
        compile(s.alt);
        branchHereFrom(endBranch);
    }

    virtual void visit(const SyntaxIf& s) {
        const auto& suites = s.branches;
        assert(suites.size() != 0);
        vector<unsigned> branchesToEnd;
        unsigned lastCondFailed = 0;
        for (unsigned i = 0; i != suites.size(); ++i) {
            if (i != 0)
                branchHereFrom(lastCondFailed);
            compile(suites[i].cond);
            lastCondFailed = emit<Instr_BranchIfFalse>();
            compile(suites[i].suite);
            branchesToEnd.push_back(emit<Instr_BranchAlways>());
        }
        branchHereFrom(lastCondFailed);

        if (s.elseSuite)
            compile(s.elseSuite);
        else
            emit<Instr_Const>(None);
        for (unsigned i = 0; i < branchesToEnd.size(); ++i)
            branchHereFrom(branchesToEnd[i]);
    }

    virtual void visit(const SyntaxWhile& s) {
        AutoSetAndRestoreOffset setLoopHead(loopHeadOffset, block->nextIndex());
        vector<unsigned> oldBreakInstrs = move(breakInstrs);
        breakInstrs.clear();
        compile(s.cond);
        unsigned branchToEnd = emit<Instr_BranchIfFalse>();
        {
            AutoPushContext enterLoop(contextStack, Context::Loop);
            compile(s.suite);
            emit<Instr_Pop>();
        }
        emit<Instr_BranchAlways>(block->offsetTo(loopHeadOffset));
        branchHereFrom(branchToEnd);
        setBreakTargets();
        breakInstrs = move(oldBreakInstrs);
        emit<Instr_Const>(None);
        // todo: else
    }

    virtual void visit(const SyntaxFor& s) {
        // 1. Get iterator
        compile(s.exprs);
        emit<Instr_GetIterator>();
        emit<Instr_GetMethod>(Names::__next__);

        // 2. Call next on iterator and break if end (loop head)
        AutoSetAndRestoreOffset setLoopHead(loopHeadOffset, block->nextIndex());
        vector<unsigned> oldBreakInstrs = move(breakInstrs);
        breakInstrs.clear();
        emit<Instr_IteratorNext>();
        unsigned exitBranch = emit<Instr_BranchIfFalse>();

        // 3. Assign results
        {
            AutoPushContext enterAssign(contextStack, Context::Assign);
            compile(s.targets);
            emit<Instr_Pop>();
        }

        // 4. Execute loop body
        {
            AutoPushContext enterLoop(contextStack, Context::Loop);
            compile(s.suite);
            emit<Instr_Pop>();
        }
        emit<Instr_BranchAlways>(block->offsetTo(loopHeadOffset));

        // 5. Exit
        branchHereFrom(exitBranch);
        emit<Instr_Pop>();

        // 6. Else clause
        if (s.elseSuite) {
            compile(s.elseSuite);
            emit<Instr_Pop>();
        }

        setBreakTargets();
        breakInstrs = move(oldBreakInstrs);
        emit<Instr_Pop>();
        emit<Instr_Pop>();
        emit<Instr_Const>(None);
    }

    void emitLambda(Name defName, const vector<Parameter>& params,
                    Traced<Block*> exprBlock, bool isGenerator)
    {
        vector<Name> names;
        unsigned defaultCount = 0;
        for (auto i = params.begin(); i != params.end(); i++) {
            names.push_back(i->name);
            if (defaultCount || i->maybeDefault) {
                assert(i->maybeDefault || i->takesRest);
                defaultCount++;
                if (i->maybeDefault)
                    compile(i->maybeDefault);
                else
                    emit<Instr_Const>(UninitializedSlot);
            }
        }
        int restParam = -1;
        for (size_t i = 0; i < params.size(); i++) {
            if (params[i].takesRest) {
                restParam = i;
                break;
            }
        }
        emit<Instr_Lambda>(defName, names, exprBlock, defaultCount, restParam,
                          isGenerator);
    }

    virtual void visit(const SyntaxLambda& s) {
        Stack<Block*> exprBlock(
            ByteCompiler().buildLambda(this, s.params, *s.expr));
        // todo: does this need to be a Name?
        Name name(internString("(lambda)"));
        emitLambda(name, s.params, exprBlock, false);
    }

    virtual void visit(const SyntaxDef& s) {
        Stack<Block*> exprBlock;
        ByteCompiler compiler;
        if (s.isGenerator)
            exprBlock = compiler.buildGenerator(this, s.params, *s.suite);
        else
            exprBlock = compiler.buildFunctionBody(this, s.params, *s.suite);
        emitLambda(s.id, s.params, exprBlock, s.isGenerator);
        compileAssign(s.id);
    }

    virtual void visit(const SyntaxClass& s) {
        Stack<Block*> suite(ByteCompiler().buildClass(this, s.id, *s.suite));
        vector<Name> params = { Names::__bases__ };
        emit<Instr_Lambda>(s.id, params, suite);
        compile(s.bases);
        emit<Instr_Call>(1);
        if (parent) {
            int slot;
            alwaysTrue(lookupLocal(s.id, slot));
            emit<Instr_SetStackLocal>(s.id, slot);
        } else {
            emit<Instr_SetGlobal>(topLevel, s.id);
        }
    }

    virtual void visit(const SyntaxAssert& s) {
        if (debugMode) {
            compile(s.cond);
            unsigned endBranch = emit<Instr_BranchIfTrue>();
            if (s.message) {
                compile(s.message);
                emit<Instr_GetMethod>(Names::__str__);
                emit<Instr_CallMethod>(0);
            } else {
                emit<Instr_Const>(None);
            }
            emit<Instr_AssertionFailed>();
            branchHereFrom(endBranch);
        }
        emit<Instr_Const>(None);
    }

    virtual void visit(const SyntaxRaise& s) {
        compile(s.right);
        emit<Instr_Raise>();
    }

    virtual void visit(const SyntaxYield& s) {
        compile(s.right);
        maybeAssertStackDepth();
        emit<Instr_SuspendGenerator>();
    }

    virtual void visit(const SyntaxTry& s) {
        unsigned finallyBranch = 0;
        if (s.finallySuite) {
            contextStack.push_back(Context::Finally);
            finallyBranch = emit<Instr_EnterFinallyRegion>();
        }
        if (s.excepts.size() != 0) {
            emitTryExcept(s);
        } else {
            compile(s.trySuite);
            emit<Instr_Pop>();
        }
        if (s.finallySuite) {
            emit<Instr_LeaveFinallyRegion>();
            branchHereFrom(finallyBranch);
            compile(s.finallySuite);
            emit<Instr_Pop>();
            emit<Instr_FinishExceptionHandler>();
            assert(contextStack.back() == Context::Finally);
            contextStack.pop_back();
        }
        emit<Instr_Const>(None);
    }

    void emitTryExcept(const SyntaxTry& s) {
        unsigned handlerBranch = emit<Instr_EnterCatchRegion>();
        compile(s.trySuite);
        emit<Instr_Pop>();
        emit<Instr_LeaveCatchRegion>();
        unsigned suiteEndBranch = emit<Instr_BranchAlways>();

        bool fullyHandled = false;
        vector<unsigned> exceptEndBranches;
        for (const auto& e : s.excepts) {
            assert(!fullyHandled);
            branchHereFrom(handlerBranch);
            if (e->expr) {
                compile(e->expr);
                emit<Instr_MatchCurrentException>();
                handlerBranch = emit<Instr_BranchIfFalse>();
                emit<Instr_HandleCurrentException>();
                if (e->as) {
                    AutoPushContext enterAssign(contextStack, Context::Assign);
                    compile(e->as);
                }
                emit<Instr_Pop>();
            } else {
                assert(!e->as);
                fullyHandled = true;
                emit<Instr_HandleCurrentException>();
                emit<Instr_Pop>();
            }
            compile(e->suite);
            emit<Instr_Pop>();
            if (e->as) {
                AutoPushContext enterDelete(contextStack, Context::Delete);
                compile(e->as);
                emit<Instr_Pop>();
            }
            exceptEndBranches.push_back(emit<Instr_BranchAlways>());
        }

        branchHereFrom(suiteEndBranch);
        if (s.elseSuite) {
            compile(s.elseSuite);
            emit<Instr_Pop>();
        }

        if (!fullyHandled)
            branchHereFrom(handlerBranch);
        emit<Instr_FinishExceptionHandler>();
        for (unsigned offset: exceptEndBranches)
            branchHereFrom(offset);
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
        unsigned offset = emit<Instr_LoopControlJump>(finallyCount);
        breakInstrs.push_back(offset);
    }

    virtual void visit(const SyntaxContinue& s) {
        if (!inLoop())
            throw ParseError(s.token, "SyntaxError: 'continue' outside loop");
        unsigned finallyCount = countFinallyBlocksInLoop();
        emit<Instr_LoopControlJump>(finallyCount, loopHeadOffset);
    }

    virtual void visit(const SyntaxImport& s) {
        // Desugar:
        //   import module as name
        //   import module.sub as name
        // into:
        //   name = __import__('module', globals(), locals(), [], 0)
        //   name = __import__('module.sub', globals(), locals(), [], 0)

        for (const auto& i : s.modules) {
            Stack<Value> value;

            value = Builtin->getAttr(Names::__import__);
            emit<Instr_Const>(value);

            value = i->name;
            emit<Instr_Const>(value);

            value = Builtin->getAttr(Names::globals);
            emit<Instr_Const>(value);
            emit<Instr_Call>(0);

            value = Builtin->getAttr(Names::locals);
            emit<Instr_Const>(value);
            emit<Instr_Call>(0);

            emit<Instr_List>(0);

            value = Value(0);
            emit<Instr_Const>(value);

            emit<Instr_Call>(5);

            compileAssign(i->localName);
            emit<Instr_Pop>();
        }
        emit<Instr_Const>(None);
    }

    virtual void visit(const SyntaxFrom& s) {
        // Desugar:
        //   from module import a, b as c
        //   from module.sub import *
        // into:
        //   temp = __import__('module', globals(), locals(), ['a', 'b'], 0)
        //   a = temp.a
        //   c = temp.b
        //
        //   temp = __import__('module.sub', globals(), locals(), [], 0)
        //   ???
        Token t = s.token;
        Stack<Value> value;

        value = Builtin->getAttr(Names::__import__);
        emit<Instr_Const>(value);

        value = s.module;
        emit<Instr_Const>(value);

        value = Builtin->getAttr(Names::globals);
        emit<Instr_Const>(value);
        emit<Instr_Call>(0);

        value = Builtin->getAttr(Names::locals);
        emit<Instr_Const>(value);
        emit<Instr_Call>(0);

        for (const auto& i : s.ids) {
            value = i->name;
            emit<Instr_Const>(value);
        }
        emit<Instr_List>(s.ids.size());

        value = Value(int32_t(s.level));
        emit<Instr_Const>(value);

        emit<Instr_Call>(5);

        if (s.ids.empty()) {
            // todo
            value = gc.create<String>("import * not implemented");
            emit<Instr_Const>(value);
            emit<Instr_AssertionFailed>();
            return;
        }

        for (const auto& i : s.ids) {
            emit<Instr_Dup>(0);
            emit<Instr_GetAttr>(i->name);
            compileAssign(i->localName);
            emit<Instr_Pop>();
        }

        emit<Instr_Pop>();
        emit<Instr_Const>(None);
    }

    virtual void visit(const SyntaxListComp& s) {
        // List comprehensions get their own scope which we implement with a
        // call to a local function.
        Stack<Block*> exprBlock(ByteCompiler().buildListComp(this, s));
        vector<Parameter> params;
        Name name(internString("(list comprehension)"));
        emitLambda(name, params, exprBlock, false);
        emit<Instr_Call>(0);
    }

    void compileListComp(const Syntax& s) {
        // Set up results variable to hold the array
#ifdef DEBUG
        if (useLexicalEnv) {
            unsigned frame;
            assert(lookupLexical(Names::listCompResult, frame));
            assert(frame == 0);
        } else {
            int slot;
            assert(lookupLocal(Names::listCompResult, slot));
            assert(slot == 0);
        }
#endif
        emit<Instr_List>(0);
        if (useLexicalEnv)
            emit<Instr_SetLexical>(0, Names::listCompResult);
        else
            emit<Instr_SetStackLocal>(Names::listCompResult, 0);
        emit<Instr_Pop>();

        // Compile expression to generate results
        compile(s);

        // Fetch result
        emit<Instr_Pop>();
        if (useLexicalEnv)
            emit<Instr_GetLexical>(0, Names::listCompResult);
        else
            emit<Instr_GetStackLocal>(Names::listCompResult, 0);
    }

    virtual void visit(const SyntaxCompIterand& s) {
        assert(kind == Kind::ListComp);
        if (useLexicalEnv)
            emit<Instr_GetLexical>(0, Names::listCompResult);
        else
            emit<Instr_GetStackLocal>(Names::listCompResult, 0);
        compile(*s.expr);
        emit<Instr_ListAppend>();
    }

    virtual void setPos(const TokenPos& pos) {
        block->setNextPos(pos);
    }
};

static bool RaiseSyntaxError(const ParseError& e, MutableTraced<Value> resultOut)
{
    ostringstream s;
    s << e.what() << " at " << e.pos.file << " line " << dec << e.pos.line;
    return Raise<SyntaxError>(s.str(), resultOut);
}

bool CompileModule(const Input& input, Traced<Env*> globalsArg,
                   MutableTraced<Value> resultOut)
{
    Stack<Env*> globals(globalsArg);
    if (!globals)
        globals = createTopLevel();

    SyntaxParser parser;
    parser.start(input);
    try {
        unique_ptr<SyntaxBlock> syntax(parser.parseModule());
        Stack<Block*> block(ByteCompiler().buildModule(globals, syntax.get()));
        resultOut = gc.create<CodeObject>(block);
    } catch (const ParseError& e) {
        RaiseSyntaxError(e, resultOut);
        return false;
    }

    return true;
}

bool CompileEval(const Input& input, Traced<Env*> globals,
                 Traced<Env*> locals, MutableTraced<Value> resultOut)
{
    assert(globals);
    assert(locals);

    SyntaxParser parser;
    parser.start(input);
    try {
        unique_ptr<Syntax> syntax(parser.parseExpr());
        Stack<Block*> block;
        block = ByteCompiler().buildEval(globals, locals, syntax.get());
        vector<Name> paramNames;
        Stack<FunctionInfo*> info(gc.create<FunctionInfo>(paramNames, block));
        resultOut = gc.create<Function>(Names::evalFuncName, info,
                                        EmptyValueArray, locals);
    } catch (const ParseError& e) {
        RaiseSyntaxError(e, resultOut);
        return false;
    }

    return true;
}

bool CompileExec(const Input& input, Traced<Env*> globals,
                 Traced<Env*> locals, MutableTraced<Value> resultOut)
{
    assert(globals);
    assert(locals);

    SyntaxParser parser;
    parser.start(input);
    try {
        unique_ptr<Syntax> syntax(parser.parseModule());
        Stack<Block*> block;
        block = ByteCompiler().buildExec(globals, locals, syntax.get());
        vector<Name> paramNames;
        Stack<FunctionInfo*> info(gc.create<FunctionInfo>(paramNames, block));
        resultOut = gc.create<Function>(Names::evalFuncName, info,
                                        EmptyValueArray, locals);
    } catch (const ParseError& e) {
        RaiseSyntaxError(e, resultOut);
        return false;
    }

    return true;
}
