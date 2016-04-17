#include "block.h"
#include "callable.h"
#include "dict.h"
#include "exception.h"
#include "frame.h"
#include "generator.h"
#include "input.h"
#include "instr.h"
#include "interp.h"
#include "list.h"
#include "name.h"
#include "repr.h"
#include "singletons.h"
#include "utils.h"

#include "value-inl.h"

#ifdef LOG_EXECUTION
bool logFrames = false;
bool logExecution = false;
#endif

#ifdef DEBUG
bool logInstrCounts = false;
size_t instrCounts[InstrCodeCount] = {0};
#endif

GlobalRoot<Block*> Interpreter::AbortTrampoline;

GlobalRoot<Interpreter*> interp;

ExceptionHandler::ExceptionHandler(Type type, unsigned stackPos, unsigned offset)
  : next_(nullptr), type_(type), stackPos_(stackPos), offset_(offset)
{}

void ExceptionHandler::setNext(ExceptionHandler* eh)
{
    assert(!next_);
    next_ = eh;
}

void ExceptionHandler::traceChildren(Tracer& t)
{
    gc.trace(t, &next_);
}

Interpreter::Interpreter()
  : instrp(nullptr),
    stack(1),
    inExceptionHandler_(false),
    jumpKind_(JumpKind::None),
    currentException_(nullptr),
    deferredReturnValue_(None),
    remainingFinallyCount_(0),
    loopControlTarget_(0)
{}

Interpreter::~Interpreter()
{
#ifdef DEBUG
    if (logInstrCounts)
        printInstrCounts();
#endif
}

void Interpreter::init()
{
    // Create a block that will cause the interpreter loop to exit.
    Stack<Env*> global;
    Stack<Block*> parent;
    AbortTrampoline.init(gc. create<Block>(parent, global, Env::InitialLayout,
                                          1, false));
    AbortTrampoline->append<Instr_Abort>();
    AbortTrampoline->setMaxStackDepth(1);

    interp.init(gc.create<Interpreter>());
}

void Interpreter::traceChildren(Tracer& t)
{
    gc.traceVector(t, &frames);
    gc.traceVector(t, &stack);
    gc.trace(t, &currentException_);
    gc.trace(t, &deferredReturnValue_);
}

bool Interpreter::exec(Traced<Block*> block, MutableTraced<Value> resultOut)
{
#ifdef LOG_EXECUTION
    if (logExecution) {
        logStart();
        cout << "exec" << endl;
    }
#endif

    AutoSetAndRestoreValue<InstrThunk*> saveInstrp(instrp, nullptr);
    pushFrame(block, stack.size(), 0);
#ifdef DEBUG
    unsigned initialSize = stack.size();
#endif
    size_t initialCapacity = stack.capacity();
    bool ok = run(resultOut);
    assert(stack.size() == initialSize);
    stack.reserve(initialCapacity); // todo: this doesn't shrink
    return ok;
}

void Interpreter::insertStackEntries(unsigned offsetFromTop, Value value,
                                     size_t count)
{
#ifdef LOG_EXECUTION
    if (logExecution) {
        logStart(1);
        cout << "insertStackEntries " << offsetFromTop << " ";
        cout << value << " " << dec << count << endl;
    }
#endif

    assert(count == 1); // todo
    stack.insert(stack.end() - offsetFromTop, value);
}

void Interpreter::eraseStackEntries(unsigned offsetFromTop, size_t count)
{
#ifdef LOG_EXECUTION
    if (logExecution) {
        logStart(1);
        cout << "eraseStackEntries " << offsetFromTop << " ";
        cout << dec << count << endl;
    }
#endif

    assert(offsetFromTop + 1 >= count);
    assert(stack.size() >= offsetFromTop + count);
    auto last = stack.end() - offsetFromTop;
    auto first = last - count;
    stack.erase(first, last);
}

#ifdef LOG_EXECUTION
static void logValue(const Value& value)
{
    AutoAssertNoGC noGC;
    if (value.isObject()) {
        Object* obj = value.asObject();
        if (!obj) {
            cout << "nullptr";
            return;
        }

        cout << obj->type()->name();
        cout << " at 0x" << hex << static_cast<void*>(obj);
        return;
    }

    cout << value;
}

void Interpreter::logStart(int indentDelta)
{
    size_t indent = frames.size() + indentDelta;
    cout << "in: ";
    for (size_t i = 0; i < indent; i++)
        cout << "  ";
}

void Interpreter::logStackPush(const Value& value)
{
    if (logExecution) {
        logStart(1);
        cout << "push @" << dec << stack.size() << " ";
        logValue(value);
        cout << endl;
    }
}

void Interpreter::logStackPush(const Value* first, const Value* last)
{
    if (logExecution) {
        size_t pos = stack.size();
        while (first != last) {
            logStart(1);
            cout << "push @" << dec << pos++ << " ";
            logValue(*first++);
            cout << endl;
        }
    }
}

void Interpreter::logStackPop(size_t count)
{
    size_t pos = stack.size();
    if (logExecution && count > 0) {
        for (size_t i = 0; i < count; i++) {
            logStart(1);
            Value value = stack[stack.size() - 1 - i];
            cout << "pop @" << dec << --pos << " ";
            logValue(value);
            cout << endl;
        }
    }
}

void Interpreter::logStackSwap()
{
    if (logExecution) {
        logStart(1);
        cout << "swap" << endl;
    }
}

void Interpreter::logInstr(Instr* instr)
{
    if (logExecution) {
        AutoAssertNoGC noGC;
        logStart();
        cout << *instr;
        cout << " at " << currentPos() << endl;
    }
}
#endif

void Interpreter::ensureStackSpace(size_t newStackSize)
{
    stack.reserve(newStackSize);
}

void Interpreter::pushFrame(Traced<Block*> block, unsigned stackStartPos,
                            unsigned extraPopCount)
{
    assert(stack.size() >= stackStartPos);
    frames.emplace_back(instrp, block, stackStartPos, extraPopCount);
    instrp = block->startInstr();

    unsigned newStackSize = stackStartPos + block->maxStackDepth();
    assert(newStackSize >= stack.size());
    ensureStackSpace(newStackSize);

#ifdef LOG_EXECUTION
    if (logFrames) {
        Frame* frame = getFrame();
        TokenPos pos = frame->block()->getPos(instrp);
        logStart(-1);
        cout << "> frame " << pos << " " << newStackSize << endl;
    }
#endif
}

void Interpreter::popFrame()
{
    assert(!frames.empty());
    Frame* frame = getFrame();

    assert(!frame->exceptionHandlers());

#ifdef LOG_EXECUTION
    if (logFrames) {
        TokenPos pos = frame->block()->getPos(instrp - 1);
        logStart(-1);
        cout << "< frame " << pos << endl;
    }
#endif

    // Update stack pos but don't resize backing vector on frame pop.
    unsigned pos = frame->stackPos() - frame->extraPopCount();
    assert(pos <= stack.size());
    stack.resize(pos); // todo: log removed values

    instrp = frame->returnPoint();
    frames.pop_back();

#ifdef LOG_EXECUTION
    if (logFrames && instrp) {
        TokenPos pos = frames.back().block()->getPos(instrp);
        logStart();
        cout << "  return to " << pos << endl;
    }
#endif
}

void Interpreter::setFrameEnv(Env* env)
{
    AutoAssertNoGC nogc;
    assert(!frames.empty());
    assert(!frames.back().env());
    frames.back().setEnv(env);
}

void Interpreter::returnFromFrame(Value value)
{
#ifdef DEBUG
    AutoAssertNoGC nogc;
#endif

    // If we are in a finally block, defer the return and run the finally suite.
    // todo: we can determine this statically
    if (startNextFinallySuite(JumpKind::Return)) {
        deferredReturnValue_ = value;
        return;
    }

    // Normal return path.
    popFrame();
    pushStack(value);
}

void Interpreter::loopControlJump(unsigned finallyCount, unsigned target)
{
    if (finallyCount) {
        alwaysTrue(startNextFinallySuite(JumpKind::LoopControl));
        remainingFinallyCount_ = finallyCount - 1;
        loopControlTarget_ = target;
        return;
    }

    instrp = getFrame()->block()->startInstr() + target;
    return;
}

GeneratorIter* Interpreter::getGeneratorIter()
{
    // GeneratorIter object stored as first stack value after any stack locals.
    Frame* frame = getFrame();
    Stack<Block*> block(frame->block());
    size_t pos =
        frame->stackPos() + block->argCount() + block->stackLocalCount();
    Stack<Value> value(stack[pos]);
    return value.as<GeneratorIter>();
}

void Interpreter::resumeGenerator(Traced<Block*> block,
                                  Traced<Env*> env,
                                  unsigned ipOffset,
                                  TracedVector<Value> savedStack,
                                  Traced<ExceptionHandler*> savedHandlers)
{
    pushFrame(block, stack.size(), 0);
    setFrameEnv(env);
    getFrame()->setHandlers(savedHandlers);
    instrp += ipOffset;
    // todo: can copy this in one go
    for (auto i = savedStack.begin(); i != savedStack.end(); i++)
        pushStack(*i);
}

// todo: this should take a MutableTracedVector, but it's not worth defining it
// for this single use.
unsigned
Interpreter::suspendGenerator(HeapVector<Value>& savedStack,
                              MutableTraced<ExceptionHandler*> savedHandlers)
{
    Frame* frame = getFrame();
    assert(frame->stackPos() <= stack.size());
    unsigned len = stack.size() - frame->stackPos();
    assert(savedStack.empty());
    savedStack.resize(len);
    // todo: probably a better way to do this with STL
    for (unsigned i = len; i != 0; i--)
        savedStack[i - 1] = popStack();
    unsigned ipOffset = instrp - frame->block()->startInstr();
    assert(!savedHandlers);
    savedHandlers = frame->takeHandlers();
    popFrame();
    return ipOffset;
}

unsigned Interpreter::currentOffset()
{
    InstrThunk* start = getFrame()->block()->startInstr();
    assert(instrp >= start + 1);
    return instrp - start - 1;
}

void Interpreter::pushExceptionHandler(ExceptionHandler::Type type,
                                       unsigned offset)
{
    assert(offset);
    // todo: Why do we store instruction offsets rather than indices?
    Stack<ExceptionHandler*> handler(
        gc.create<ExceptionHandler>(type, stack.size(), offset + currentOffset()));
    getFrame()->pushHandler(handler);
}

void Interpreter::popExceptionHandler(ExceptionHandler::Type type)
{
    ExceptionHandler* eh = getFrame()->popHandler();
    assert(eh->type() == type);
    (void)eh;
}

void Interpreter::raiseException(Traced<Value> exception)
{
    pushStack(exception);
    raiseException();
}

void Interpreter::raiseException()
{
    if (!handleException())
        pushFrame(AbortTrampoline, stack.size() - 1, 0);
}

bool Interpreter::handleException()
{
    Stack<Value> value(popStack());

#ifdef LOG_EXECUTION
    if (logExecution) {
        logStart(1);
        cout << "exception " << value << endl;
    }
#endif

    assert(value.isInstanceOf(Exception::ObjectClass));
    Stack<Exception*> exception(value.as<Exception>());
    if (!exception->hasTraceback())
        exception->recordTraceback(instrp ? instrp - 1 : nullptr);
    if (startExceptionHandler(exception))
        return true;

    while (instrp)
        popFrame();
    pushStack(value);
    return false;
}

bool Interpreter::startExceptionHandler(Traced<Exception*> exception)
{
    while (!getFrame()->exceptionHandlers()) {
        popFrame();
        if (!instrp)
            return false;
    }

    ExceptionHandler* handler = getFrame()->popHandler();
    instrp = getFrame()->block()->startInstr() + handler->offset();
    assert(handler->stackPos() <= stack.size());
    stack.resize(handler->stackPos());
    inExceptionHandler_ = true;
    jumpKind_ = JumpKind::Exception;
    currentException_ = exception;
    return true;
}

bool Interpreter::startNextFinallySuite(JumpKind jumpKind)
{
    assert(jumpKind == JumpKind::Return || jumpKind == JumpKind::LoopControl);

    Frame* frame = getFrame();
    while (frame->exceptionHandlers())
    {
        ExceptionHandler* handler = frame->popHandler();
        if (handler->type() == ExceptionHandler::FinallyHandler) {
            inExceptionHandler_ = true;
            jumpKind_ = jumpKind;
            instrp = getFrame()->block()->startInstr() + handler->offset();
            return true;
        }
    }

    return false;
}

bool Interpreter::isHandlingException() const
{
    return inExceptionHandler_ && jumpKind_ == JumpKind::Exception;
}

bool Interpreter::isHandlingDeferredReturn() const
{
    return inExceptionHandler_ && jumpKind_ == JumpKind::Return;
}

bool Interpreter::isHandlingLoopControl() const
{
    return inExceptionHandler_ && jumpKind_ == JumpKind::LoopControl;
}

Exception* Interpreter::currentException()
{
    assert(isHandlingException());
    assert(currentException_);
    return currentException_;
}

void Interpreter::maybeContinueHandlingException()
{
    switch (jumpKind_) {
      case JumpKind::Exception: {
        // Re-raise current exception.
        assert(currentException_);
        Stack<Exception*> exception(currentException_);
        finishHandlingException();
        pushStack(exception);
        raiseException();
        break;
      }

      case JumpKind::Return: {
        Stack<Value> value(deferredReturnValue_);
        finishHandlingException();
        returnFromFrame(value);
        break;
      }

      case JumpKind::LoopControl: {
        unsigned finallyCount = remainingFinallyCount_;
        unsigned offset = loopControlTarget_;
        finishHandlingException();
        loopControlJump(finallyCount, offset);
        break;
      }

      case JumpKind::None:
        break;
    }
}

void Interpreter::finishHandlingException()
{
    assert(inExceptionHandler_);
    inExceptionHandler_ = false;
    jumpKind_ = JumpKind::None;
    currentException_ = nullptr;
    deferredReturnValue_ = None;
    remainingFinallyCount_ = 0;
    loopControlTarget_ = 0;
}

Env* Interpreter::lexicalEnv(unsigned index)
{
    Stack<Env*> env(this->env());
    assert(env);
    for (unsigned i = 0 ; i < index; i++) {
        env = env->parent();
        assert(env);
    }
    return env;
}

void Interpreter::branch(int offset)
{
    InstrThunk* target = instrp + offset - 1;
    assert(getFrame()->block()->contains(target));
    instrp = target;
}

TokenPos Interpreter::currentPos()
{
    assert(instrp);
    assert(!frames.empty());
    return getFrame()->block()->getPos(instrp - 1);
}

void Interpreter::raiseAttrError(Traced<Value> value, Name ident)
{
    const Class* cls = value.toObject()->type();
    string message =
        "'" + cls->name() + "' object has no attribute '" + ident + "'";
    raise<AttributeError>(message);
}

void Interpreter::raiseNameError(Name ident)
{
    string message = string("name '") + ident + "' is not defined";
    raise<NameError>(message);
}

bool Interpreter::syncCall(Traced<Value> targetValue, NativeArgs args,
                           MutableTraced<Value> resultOut)
{
    unsigned argCount = args.size();
    for (unsigned i = 0; i < argCount; i++)
        pushStack(args[i]);
    return syncCall(targetValue, argCount, resultOut);
}

bool Interpreter::syncCall(Traced<Value> targetValue, unsigned argCount,
                           MutableTraced<Value> resultOut)
{
    // Synchronous call while we may already be in the interpreter.
    // Save the current instruction pointer and set it to null to so the
    // interpreter loop knows to exit rather than resume the the previous frame.
    AutoSetAndRestoreValue<InstrThunk*> saveInstrp(instrp, nullptr);

    CallStatus status = setupCall(targetValue, argCount, Layout::Empty, 0,
                                  resultOut);
    if (status != CallStarted) {
        popStack(argCount);
        return status == CallFinished;
    }

#ifdef DEBUG
    unsigned initialSize = stack.size() - getFrame()->block()->argCount();
#endif
    bool ok = run(resultOut);
    assert(stack.size() == initialSize - 1);
    return ok;
}

void Interpreter::startCall(Traced<Value> targetValue,
                            unsigned argCount,
                            Traced<Layout*> keywordArgs,
                            unsigned extraPopCount)
{
    Stack<Value> result;
    CallStatus status = setupCall(targetValue, argCount, keywordArgs,
                                  extraPopCount, result);
    if (status != CallStarted) {
        popStack(argCount);
        popStack(extraPopCount);
        pushStack(result);
        if (status == CallError)
            raiseException();
    }
}

bool Interpreter::failWithArgumentError(Traced<Callable*> callable,
                                        unsigned count,
                                        MutableTraced<Value> resultOut)
{
    ostringstream s;
    s << callable->name() << "() takes " << dec;
    if (callable->minArgs() != callable->maxArgs())
        s << "at least ";
    s << callable->minArgs();
    s << " positional arguments but " << count;
    if (count == 1)
        s << " was given";
    else
        s << " were given";
    failWithTypeError(s.str(), resultOut);
    return false;
}

Interpreter::CallStatus
Interpreter::failWithTypeError(string message, MutableTraced<Value> resultOut)
{
    Raise<TypeError>(message, resultOut);
    return CallError;
}

inline bool Interpreter::checkArguments(Traced<Callable*> callable,
                                        const TracedVector<Value>& args,
                                        MutableTraced<Value> resultOut)
{
    unsigned count = args.size();
    if (count < callable->minArgs() || count > callable->maxArgs())
        return failWithArgumentError(callable, count, resultOut);

    return true;
}

void Interpreter::mungeSimpleArguments(Traced<Function*> function,
                                       unsigned argCount)
{
    // Input stack:
    //
    //   +-------------+-------+-------+-------+
    //   | Call target | Arg 0 |  ...  | Arg n |
    //   +-------------+-------+-------+-------+
    //
    // Output stack:
    //
    //   +-------------+-------+-------+-------+-------+-------+
    //   | Call target | Arg 0 |  ...  | Rest? |  ...  | Arg m |
    //   +-------------+-------+-------+-------+-------+-------+

    auto argsStart = stack.end() - argCount;

    Stack<Value> restArg(Tuple::Empty);
    if (function->takesRest() && argCount >= function->restParam()) {
        size_t restCount = argCount - function->restParam();
        restArg = Tuple::get(stackSlice(restCount));
    }

    // todo: do we still need to do this?
    if (argCount > function->argCount()) {
        auto argsEnd = argsStart + function->argCount();
        stack.erase(argsEnd, stack.end());
    } else if (argCount < function->argCount()) {
        size_t count = function->argCount() - argCount;
        stack.insert(argsStart + argCount, UninitializedSlot.get(), count);
    }

    TracedVector<Value> args(stackSlice(function->argCount()));

    // Fill in the rest argument if taken.
    if (function->takesRest())
        args[function->restParam()] = restArg;

    // Fill in the keywords argument if taken.
    if (function->takesKeywords())
        args[function->keywordsParam()] = gc.create<Dict>();

    // Fill unfilled slots from default args.
    // todo: could store rest param like this to start with
    size_t defaultEnd = function->argCount();
    if (function->takesKeywords())
        defaultEnd--;
    size_t restParamOrEnd =
        function->takesRest() ? function->restParam() : defaultEnd;
    for (size_t i = argCount; i < restParamOrEnd; i++)
        args[i] = function->paramDefault(i);
    for (size_t i = restParamOrEnd + 1; i < defaultEnd; i++)
        args[i] = function->paramDefault(i);
}

bool Interpreter::mungeFullArguments(Traced<Function*> function,
                                     Traced<Layout*> keywordArgs,
                                     unsigned argCount,
                                     MutableTraced<Value> resultOut)
{
    // Input stack:
    //
    //   +-------------+-------+-------+-------+------+------+------+
    //   | Call target | Arg 0 |  ...  | Arg n | KW 0 |  ..  | KW m |
    //   +-------------+-------+-------+-------+------+------+------+
    //
    // Intermediate state:
    //
    //   --+-------+-------+-------+-------+-------+------+------+------+
    //     | Arg 0 |  ...  | Rest? |  ...  | Arg m | KW 0 |  ..  | KW m |
    //   --+-------+-------+-------+-------+-------+------+------+------+
    //
    // Output stack:
    //
    //   +-------------+-------+-------+-------+-------+-------+
    //   | Call target | Arg 0 |  ...  | Rest? |  ...  | Arg m |
    //   +-------------+-------+-------+-------+-------+-------+

    size_t keywordCount = keywordArgs->slotCount();
    auto argsStart = stack.end() - keywordCount - argCount;

    Stack<Value> restArg(Tuple::Empty);
    if (function->takesRest() && argCount >= function->restParam()) {
        size_t restCount = argCount - function->restParam();
        restArg = Tuple::get(stackSlice(restCount, keywordCount));
    }

    // If there are more arguments supplied than formal args, move the keyword
    // arguments down the stack. Alternatively, if there are fewer supplied
    // arguments than formal args then move keyword arguments up the stack so
    // there is enough space for all formal arguments, filling unsupplied
    // positional args with UninitializedSlot and setting any rest argument to
    // the empty tuple.
    if (argCount > function->argCount()) {
        auto argsEnd = argsStart + function->argCount();
        stack.erase(argsEnd, stack.end() - keywordCount);
    } else if (argCount < function->argCount()) {
        size_t count = function->argCount() - argCount;
        stack.insert(argsStart + argCount, UninitializedSlot.get(), count);

        if (function->takesRest()) {
            for (size_t i = function->restParam() + 1; i < argCount; i++)
                *(argsStart + i) = UninitializedSlot.get();
        }
    }

    // Fill in the rest argument if taken.
    TracedVector<Value> args(stackSlice(function->argCount(), keywordCount));
    if (function->takesRest())
        args[function->restParam()] = restArg;

    // For each supplied keyword arg, look up its position in the formal arg
    // list and set its value.  Raise TypeError if slot already filled or if the
    // argument is not found.
    //
    // todo: add to mapping dictionary for unknown keyword args if the function
    // can receive that
    Dict* keywordDict = nullptr;
    if (function->takesKeywords())
        keywordDict = gc.create<Dict>();
    if (keywordCount) {
        TracedVector<Value> keywords(stackSlice(keywordCount));
        Layout* l = keywordArgs;
        for (size_t i = 0; i < keywordCount; i++)
        {
            int argPos = function->findArg(l->name());
            if (argPos == -1 ||
                (function->takesRest() &&
                 argPos == function->restParam()) ||
                (function->takesKeywords() &&
                 argPos == function->keywordsParam()))
            {
                if (keywordDict) {
                    Stack<Value> key(l->name());
                    Stack<Value> value(keywords[i]);
                    keywordDict->setitem(key, value);
                } else {
                    return Raise<TypeError>("Unexpected keyword arg: " +
                                            l->name()->value(),
                                            resultOut);
                }
            } else {
                // todo: lookup might be easier if formal arg list was a layout
                if (args[argPos] != UninitializedSlot.get()) {
                    return Raise<TypeError>(
                        "Multiple values for argument: " + l->name()->value(),
                        resultOut);
                }
                args[argPos] = keywords[i];
            }
            l = l->parent();
        }
    }
    popStack(keywordCount);
    if (function->takesKeywords())
        args[function->keywordsParam()] = keywordDict;

    // Fill unfilled slots from default args.  If there are unfilled slots
    // remaining, raise TypeError.
    size_t firstDefaultArg = function->firstDefaultParam();
    for (size_t i = argCount; i < firstDefaultArg; i++) {
        if (args[i] == UninitializedSlot.get()) {
            return Raise<TypeError>("Missing argument: " +
                                    function->paramName(i)->value(), resultOut);
        }
    }
    for (size_t i = firstDefaultArg; i < function->argCount(); i++) {
        if (args[i] == UninitializedSlot.get())
            args[i] = function->paramDefault(i);
    }

    return true;
}

Interpreter::CallStatus Interpreter::setupCall(Traced<Value> targetValue,
                                               unsigned argCount,
                                               Traced<Layout*> keywordArgs,
                                               unsigned extraPopCount,
                                               MutableTraced<Value> resultOut)
{
#ifdef LOG_EXECUTION
    if (logExecution) {
        logStart(1);
        cout << "setupCall @" << dec << stack.size() << " " << targetValue.get();
        cout << " " << argCount << " " << extraPopCount << endl;
    }
#endif

    Stack<Object*> target(targetValue.toObject());
    if (target->is<Native>()) {
        if (keywordArgs != Layout::Empty) {
            return failWithTypeError(
                "Native function takes no keyword arguments", resultOut);
        }
        Stack<Native*> native(target->as<Native>());
        NativeArgs args(stackSlice(argCount));
        if (!checkArguments(native, args, resultOut))
            return CallError;
        bool ok = native->call(args, resultOut);
        return ok ? CallFinished : CallError;
    } else if (target->is<Function>()) {
        Stack<Function*> function(target->as<Function>());
        if (!checkArguments(function, stackSlice(argCount), resultOut))
            return CallError;
        Stack<Block*> block(function->block());
        if (keywordArgs == Layout::Empty) {
            mungeSimpleArguments(function, argCount);
        } else {
            if (!mungeFullArguments(function, keywordArgs, argCount, resultOut))
                return CallError;
        }
        pushFrame(block, stack.size() - function->argCount(), extraPopCount);
        Stack<Env*> parentEnv(function->env());
        pushStack(parentEnv);
        return CallStarted;
    } else if (target->is<Class>()) {
        // todo: this is messy and needs refactoring
        Stack<Class*> cls(target->as<Class>());
        Stack<Value> func;
        if (target->maybeGetAttr(Names::__new__, func) && !func.isNone()) {
            insertStackEntries(argCount, Value(cls));
            TracedVector<Value> funcArgs(stackSlice(argCount + 1));
            if (!syncCall(func, funcArgs, resultOut)) {
                return CallError;
            }
            if (resultOut.isInstanceOf(cls)) {
                Stack<Value> initFunc;
                if (target->maybeGetAttr(Names::__init__, initFunc)) {
                    Stack<Value> initResult;
                    funcArgs[0] = resultOut;
                    if (!syncCall(initFunc, funcArgs, initResult)) {
                        resultOut = initResult;
                        return CallError;
                    }
                    if (initResult.get().toObject() != None) {
                        return failWithTypeError(
                            "__init__() should return None", resultOut);
                    }
                }
            }
        } else {
            string message = "cannot create '" + cls->name() + "' instances";
            return failWithTypeError(message, resultOut);
        }
        popStack();
        return CallFinished;
    } else if (target->is<Method>()) {
        Stack<Method*> method(target->as<Method>());
        Stack<Value> callable(method->callable());
        insertStackEntries(argCount, method->object());
        return setupCall(callable, argCount + 1, keywordArgs, extraPopCount,
                         resultOut);
    } else {
        Stack<Value> callHook;
        if (!getAttr(targetValue, Names::__call__, callHook)) {
            return failWithTypeError(
                "object is not callable:" + repr(target), resultOut);
        }

        return setupCall(callHook, argCount, keywordArgs, extraPopCount,
                         resultOut);
    }
}

const Heap<Instr*>& Interpreter::currentInstr() const
{
    return instrp[-1].data;
}

void Interpreter::insertStubInstr(Instr* current, Instr* stub)
{
    InstrThunk& it = instrp[-1];
    assert(getNextInstr(stub) == it.data);
    assert(getFinalInstr(it.data) == current);
    current->incStubCount();
    it.code = stub->code();
    it.data = stub;
}

void Interpreter::replaceAllStubs(Instr* current, Instr* stub)
{
    // This replaces all stubs but doesn't reset the stub count.
    InstrThunk& it = instrp[-1];
    assert(getNextInstr(stub) == current);
    assert(getFinalInstr(it.data) == current);
    current->incStubCount();
    it.code = stub->code();
    it.data = stub;
}

void Interpreter::printInstrCounts()
{
    cout << dec;
    printf("Instruction count stats\n");
    for (size_t i = 0; i < InstrCodeCount; i++)
        printf("  %25s: %ld\n", instrName(InstrCode(i)), instrCounts[i]);
}
