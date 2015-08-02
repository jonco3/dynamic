#include "block.h"
#include "callable.h"
#include "exception.h"
#include "frame.h"
#include "generator.h"
#include "input.h"
#include "instr.h"
#include "interp.h"
#include "name.h"
#include "repr.h"
#include "utils.h"

#include "value-inl.h"

#ifdef LOG_EXECUTION
bool logFrames = false;
bool logExecution = false;
#endif

Interpreter interp;

ExceptionHandler::ExceptionHandler(Type type, unsigned frameIndex, unsigned offset)
  : type_(type), frameIndex_(frameIndex), offset_(offset)
{}

Interpreter::Interpreter()
  : instrp(nullptr),
    inExceptionHandler_(false),
    jumpKind_(JumpKind::None),
    currentException_(nullptr),
    deferredReturnValue_(None),
    remainingFinallyCount_(0),
    loopControlTarget_(0)
{}

bool Interpreter::exec(Traced<Block*> block, MutableTraced<Value> resultOut)
{
#ifdef LOG_EXECUTION
    if (logExecution) {
        logStart();
        cout << "exec" << endl;
    }
#endif

    assert(stackPos() == 0);
    assert(frames.empty());
    pushFrame(block, 0);
#ifdef DEBUG
    unsigned initialPos = stackPos();
#endif
    bool ok = run(resultOut);
    assert(stackPos() == initialPos);
    return ok;
}


void Interpreter::removeStackEntries(unsigned offsetFromTop, unsigned count)
{
    size_t initialSize = stack.size();

#ifdef LOG_EXECUTION
    if (logExecution) {
        logStart(1);
        cout << "removeStackEntries @" << (initialSize - offsetFromTop);
        cout << " " << count << endl;
    }
#endif

    assert(offsetFromTop + count <= initialSize);
    auto end = stack.begin() + (initialSize - offsetFromTop);
    auto begin = end - count;
    stack.erase(begin, end);
    assert(stack.size() == initialSize - count);
}

void Interpreter::insertStackEntry(unsigned offsetFromTop, Value value)
{
    size_t initialSize = stack.size();

#ifdef LOG_EXECUTION
    if (logExecution) {
        logStart(1);
        cout << "insertStackEntry @" << (initialSize - offsetFromTop);
        cout << " " << value << endl;
    }
#endif

    auto pos = stack.begin() + (initialSize - offsetFromTop);
    stack.insert(pos, value);
}

Value Interpreter::getStackLocal(unsigned offset)
{
    Frame* frame = getFrame();
    assert(offset < frame->block()->layout()->slotCount());
    return stack[frame->stackPos() + offset];
}

void Interpreter::setStackLocal(unsigned offset, Traced<Value> value)
{
    Frame* frame = getFrame();
    assert(offset < frame->block()->layout()->slotCount());
    stack[frame->stackPos() + offset] = value;
}

#ifdef LOG_EXECUTION
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
        cout << "push @" << stackPos() << " " << value << endl;
    }
}

void Interpreter::logStackPop(size_t count)
{
    size_t pos = stackPos();
    if (logExecution && count > 0) {
        for (size_t i = 0; i < count; i++) {
            logStart(1);
            Value value = stack[stack.size() - 1 - i];
            cout << "pop @" << --pos << " " << value << endl;
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
#endif

bool Interpreter::run(MutableTraced<Value> resultOut)
{
    Stack<Instr*> instr;
    InstrFuncBase func;
    while (instrp) {
        assert(getFrame()->block()->contains(instrp));
        func = instrp->func;
        instr = instrp->data;
        instrp++;
#ifdef LOG_EXECUTION
        if (logExecution) {
            logStart();
            cout << *instr << " at line " << dec << currentPos().line << endl;
        }
#endif
        if (!func(instr, *this)) {
            Stack<Value> value(popStack());
#ifdef LOG_EXECUTION
            if (logExecution) {
                logStart(1);
                cout << "exception " << value << endl;
            }
#endif
            assert(value.isInstanceOf(Exception::ObjectClass));
            Stack<Exception*> exception(value.toObject()->as<Exception>());
            if (!exception->hasPos() && instrp)
                exception->setPos(currentPos());
            if (!startExceptionHandler(exception)) {
                while (instrp)
                    popFrame();
                resultOut = value;
                return false;
            }
        }
    }

    resultOut = popStack();
    return true;
}

unsigned Interpreter::frameIndex()
{
    assert(!frames.empty());
    return frames.size() - 1;
}

void Interpreter::pushFrame(Traced<Block*> block, unsigned stackStartPos)
{
    assert(stackPos() >= stackStartPos);
    frames.emplace_back(instrp, block, stackStartPos);
    instrp = block->startInstr();

#ifdef LOG_EXECUTION
    if (logFrames) {
        Frame* frame = getFrame();
        TokenPos pos = frame->block()->getPos(instrp);
        logStart(-1);
        cout << "> frame " << pos << endl;
    }
#endif
}

void Interpreter::setFrameEnv(Traced<Env*> env)
{
    assert(!frames.empty());
    assert(!frames.back().env());
    frames.back().setEnv(env);
}

void Interpreter::popFrame()
{
    assert(!frames.empty());
    Frame* frame = getFrame();

#ifdef DEBUG
    for (const auto handler : exceptionHandlers)
        assert(handler->frameIndex() < frameIndex());
    assert(frame->stackPos() <= stackPos());
#endif

#ifdef LOG_EXECUTION
    if (logFrames) {
        TokenPos pos = frame->block()->getPos(instrp - 1);
        logStart(-1);
        cout << "< frame " << pos << endl;
    }
#endif

    popStack(stackPos() - frame->stackPos());
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
                                  vector<Value>& savedStack)
{
    popFrame();
    pushFrame(block, stackPos());
    setFrameEnv(env);
    instrp += ipOffset;
    // todo: can copy this in one go
    for (auto i = savedStack.begin(); i != savedStack.end(); i++)
        pushStack(*i);
    savedStack.resize(0);
}

unsigned Interpreter::suspendGenerator(vector<Value>& savedStack)
{
    Frame* frame = getFrame();
    assert(frame->stackPos() <= stackPos());
    unsigned len = stackPos() - frame->stackPos();
    assert(savedStack.empty());
    savedStack.resize(len);
    // todo: probably a better way to do this with STL
    for (unsigned i = 0; i < len; i++)
        savedStack[i] = stack[i + frame->stackPos()];
    unsigned ipOffset = instrp - frame->block()->startInstr();
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
    assert(exceptionHandlers.empty() ||
           exceptionHandlers.back()->frameIndex() <= frameIndex());
    // todo: Why do we store instruct offsets rather than indices?
    Stack<ExceptionHandler*> handler(
        gc.create<ExceptionHandler>(type, frameIndex(), offset + currentOffset()));
    exceptionHandlers.push_back(handler);
}

void Interpreter::popExceptionHandler(ExceptionHandler::Type type)
{
#ifdef DEBUG
    ExceptionHandler* handler = exceptionHandlers.back();
    assert(handler->frameIndex() == frameIndex());
    assert(handler->type() == type);
#endif
    exceptionHandlers.pop_back();
}

bool Interpreter::startExceptionHandler(Traced<Exception*> exception)
{
    if (exceptionHandlers.empty())
        return false;

    Stack<ExceptionHandler*> handler(exceptionHandlers.back());
    while (frameIndex() != handler->frameIndex() && instrp)
        popFrame();
    if (!instrp)
        return false;

    exceptionHandlers.pop_back();
    instrp = getFrame()->block()->startInstr() + handler->offset();
    inExceptionHandler_ = true;
    jumpKind_ = JumpKind::Exception;
    currentException_ = exception;
    return true;
}

bool Interpreter::startNextFinallySuite(JumpKind jumpKind)
{
    assert(jumpKind == JumpKind::Return || jumpKind == JumpKind::LoopControl);
    while (ExceptionHandler* handler = currentExceptionHandler()) {
        if (handler->frameIndex() != frameIndex())
            break;
        exceptionHandlers.pop_back();
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

bool Interpreter::maybeContinueHandlingException()
{
    switch (jumpKind_) {
      case JumpKind::Exception: {
        // Re-raise current exception.
        assert(currentException_);
        Stack<Exception*> exception(currentException_);
        finishHandlingException();
        if (!startExceptionHandler(exception)) {
            pushStack(exception);
            return false;
        }
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

      default:
        break;
    }

    return true;
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

ExceptionHandler* Interpreter::currentExceptionHandler()
{
    if (exceptionHandlers.empty())
        return nullptr;

    ExceptionHandler* handler = exceptionHandlers.back();
    if (handler->frameIndex() != frameIndex())
        return nullptr;

    return handler;
}

Frame* Interpreter::getFrame(unsigned reverseIndex)
{
    assert(reverseIndex < frames.size());
    return &frames[frameIndex() - reverseIndex];
}

Env* Interpreter::lexicalEnv(unsigned index)
{
    Stack<Env*> env(this->env());
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

bool Interpreter::raiseAttrError(Traced<Value> value, Name ident)
{
    const Class* cls = value.toObject()->type();
    string message =
        "'" + cls->name() + "' object has no attribute '" + ident + "'";
    pushStack(gc.create<AttributeError>(message));
    return false;
}

bool Interpreter::raiseNameError(Name ident)
{
    string message = "name '" + ident + "' is not defined";
    pushStack(gc.create<NameError>(message));
    return false;
}

bool Interpreter::call(Traced<Value> targetValue, TracedVector<Value> args,
                       MutableTraced<Value> resultOut)
{
    unsigned argCount = args.size();
    for (unsigned i = 0; i < argCount; i++)
        pushStack(args[i]);
    return call(targetValue, argCount, resultOut);
}

bool Interpreter::call(Traced<Value> targetValue, unsigned argCount,
                       MutableTraced<Value> resultOut)
{
    // Synchronous call while we may already be in the interpreter.
    // Save the current instruction pointer and set it to null to so the
    // interpreter loop knows to exit rather than resume the the previous frame.
    AutoSetAndRestoreValue<InstrThunk*> saveInstrp(instrp, nullptr);

    CallStatus status = setupCall(targetValue, argCount, resultOut);
    if (status != CallStarted) {
        popStack(argCount);
        return status == CallFinished;
    }

#ifdef DEBUG
    unsigned initialPos = stackPos() - getFrame()->block()->argCount();
#endif
    bool ok = run(resultOut);
    assert(stackPos() == initialPos - 1);
    return ok;
}

bool Interpreter::startCall(Traced<Value> targetValue, unsigned argCount)
{
    Stack<Value> result;
    CallStatus status = setupCall(targetValue, argCount, result);
    if (status != CallStarted) {
        popStack(argCount);
        pushStack(result);
        return status == CallFinished;
    }

    return true;
}

bool Interpreter::checkArguments(Traced<Callable*> callable,
                                 const TracedVector<Value>& args,
                                 MutableTraced<Value> resultOut)
{
    unsigned count = args.size();
    if (count < callable->minArgs() || count > callable->maxArgs()) {
        ostringstream s;
        s << callable->name() << "() takes " << dec;
        if (callable->minArgs() != callable->maxArgs())
            s << "at least ";
        s << callable->minArgs();
        s << " positional arguments but " << args.size();
        if (args.size() == 1)
            s << " was given";
        else
            s << " were given";
        return raiseTypeError(s.str(), resultOut);
    }
    return true;
}

unsigned Interpreter::mungeArguments(Traced<Function*> function,
                                     unsigned argCount)
{
    // Fill in default values for missing arguments
    while (argCount < function->maxNormalArgs())
        pushStack(function->paramDefault(argCount++));

    // Add rest argument tuple if necessary
    assert(argCount >= function->maxNormalArgs());
    if (function->takesRest()) {
        size_t restCount = argCount - function->maxNormalArgs();
        TracedVector<Value> restArgs(stackSlice(restCount));
        Stack<Value> rest(Tuple::get(restArgs));
        popStack(restCount);
        pushStack(rest);
        argCount = argCount - restCount + 1;
    }

    return argCount;
}

Interpreter::CallStatus Interpreter::setupCall(Traced<Value> targetValue,
                                               TracedVector<Value> args,
                                               MutableTraced<Value> resultOut)
{
    unsigned count = args.size();
    for (unsigned i = 0; i < count; i++)
        pushStack(args[i]);
    return setupCall(targetValue, count, resultOut);
}

Interpreter::CallStatus Interpreter::setupCall(Traced<Value> targetValue,
                                               unsigned argCount,
                                               MutableTraced<Value> resultOut)
{
#ifdef LOG_EXECUTION
    if (logExecution) {
        logStart(1);
        cout << "setupCall @" << stackPos() << " " << targetValue.get();
        cout<< " " << argCount << endl;
    }
#endif

    Stack<Object*> target(targetValue.toObject());
    if (target->is<Native>()) {
        Stack<Native*> native(target->as<Native>());
        TracedVector<Value> args(stackSlice(argCount));
        if (!checkArguments(native, args, resultOut))
            return CallError;
        bool ok = native->call(args, resultOut);
        return ok ? CallFinished : CallError;
    } else if (target->is<Function>()) {
        Stack<Function*> function(target->as<Function>());
        TracedVector<Value> args(stackSlice(argCount));
        if (!checkArguments(function, args, resultOut))
            return CallError;
        Stack<Block*> block(function->block());
        argCount = mungeArguments(function, argCount);
        assert(argCount == function->argCount());
        pushFrame(block, stackPos() - argCount);
        Stack<Env*> parentEnv(function->env());
        pushStack(parentEnv);
        return CallStarted;
    } else if (target->is<Class>()) {
        // todo: this is messy and needs refactoring
        Stack<Class*> cls(target->as<Class>());
        Stack<Value> func;
        if (!target->maybeGetAttr(Name::__new__, func) || func.isNone()) {
            string message = "cannot create '" + cls->name() + "' instances";
            return raiseTypeError(message, resultOut);
        }
        RootVector<Value> funcArgs(stackSlice(argCount));
        funcArgs.insert(funcArgs.begin(), Value(cls));
        if (!call(func, funcArgs, resultOut))
            return CallError;
        if (resultOut.isInstanceOf(cls)) {
            Stack<Value> initFunc;
            if (target->maybeGetAttr(Name::__init__, initFunc)) {
                Stack<Value> initResult;
                funcArgs[0] = resultOut;
                if (!call(initFunc, funcArgs, initResult))
                    return CallError;
                if (initResult.get().toObject() != None) {
                    return raiseTypeError(
                        "__init__() should return None", resultOut);
                }
            }
        }
        return CallFinished;
    } else if (target->is<Method>()) {
        Stack<Method*> method(target->as<Method>());
        Stack<Value> callable(method->callable());
        insertStackEntry(argCount, method->object());
        return setupCall(callable, argCount + 1, resultOut);
    } else {
        // todo: this is untested
        Stack<Value> callHook;
        if (!getAttr(targetValue, Name::__call__, callHook)) {
            return raiseTypeError(
                "object is not callable:" + repr(target), resultOut);
        }

        return setupCall(callHook, argCount, resultOut);
    }
}

Interpreter::CallStatus Interpreter::raiseTypeError(string message,
                                                    MutableTraced<Value> resultOut)
{
    resultOut = gc.create<TypeError>(message);
    return CallError;
}

void Interpreter::replaceInstr(Instr* current,
                               InstrFuncBase newFunc,
                               Instr* newData)
{
    InstrThunk& it = instrp[-1];
    assert(it.data == current);
    it.func = newFunc;
    it.data = newData;
}

void Interpreter::replaceInstrFunc(Instr* current,
                                   InstrFuncBase newFunc)
{
    InstrThunk& it = instrp[-1];
    assert(it.data == current);
    it.func = newFunc;
}

bool Interpreter::replaceInstrAndRestart(Instr* current,
                                         InstrFuncBase newFunc,
                                         Instr* newData)
{
    replaceInstr(current, newFunc, newData);
    Stack<Instr*> instr(newData);
    return newFunc(instr, *this);
}

bool Interpreter::replaceInstrFuncAndRestart(Instr* current,
                                             InstrFuncBase newFunc)
{
    replaceInstrFunc(current, newFunc);
    Stack<Instr*> instr(current);
    return newFunc(instr, *this);
}
