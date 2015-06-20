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

ExceptionHandler::ExceptionHandler(Type type, unsigned frameIndex, unsigned offset)
  : type_(type), frameIndex_(frameIndex), offset_(offset)
{}

Interpreter* Interpreter::instance_ = nullptr;

/* static */ void Interpreter::init()
{
    assert(!instance_);
    instance_ = new Interpreter;
}

/* static */ bool Interpreter::exec(Traced<Block*> block, MutableTraced<Value> resultOut)
{
    assert(instance_);
    return instance_->interpret(block, resultOut);
}

Interpreter::Interpreter()
  : instrp(nullptr),
    inExceptionHandler_(false),
    jumpKind_(JumpKind::None),
    currentException_(nullptr),
    deferredReturnValue_(None),
    remainingFinallyCount_(0),
    loopControlTarget_(0)
{}

bool Interpreter::interpret(Traced<Block*> block, MutableTraced<Value> resultOut)
{
    assert(stackPos() == 0);
    Stack<Env*> nullParent;
    Stack<Env*> env(gc.create<Env>(nullParent, block));
    pushFrame(block, env);
    return run(resultOut);
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
        cout << "push " << value << endl;
    }
}

void Interpreter::logStackPop(size_t count)
{
    if (logExecution && count > 0) {
        for (size_t i = 0; i < count; i++) {
            logStart(1);
            cout << "pop " << stack[stack.size() - 1 - i] << endl;
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
#ifdef DEBUG
    unsigned initialPos = stackPos();
#endif
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
            exception->setPos(currentPos());
            if (!startExceptionHandler(exception)) {
                while (instrp)
                    popFrame();
                resultOut = value;
                assert(stackPos() == initialPos);
                return false;
            }
        }
    }

    resultOut = popStack();
    assert(stackPos() == initialPos);
    return true;
}

unsigned Interpreter::frameIndex()
{
    assert(!frames.empty());
    return frames.size() - 1;
}

void Interpreter::pushFrame(Traced<Block*> block, Traced<Env*> env)
{
    instrp = block->startInstr();
    frames.emplace_back(block, env, stackPos());

#ifdef LOG_EXECUTION
    if (logFrames) {
        Frame* frame = getFrame();
        TokenPos pos = frame->block()->getPos(instrp);
        logStart(-1);
        cout << "> frame " << pos << endl;
    }
#endif
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

    stack.resize(frame->stackPos());
    instrp = frame->returnPoint();
    frames.pop_back();
}

#ifdef DEBUG
unsigned Interpreter::frameStackDepth()
{
    assert(!frames.empty());
    Frame* frame = getFrame();
    return stackPos() - frame->stackPos();
}
#endif

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

void Interpreter::resumeGenerator(Traced<Block*> block,
                                  Traced<Env*> env,
                                  unsigned ipOffset,
                                  vector<Value>& savedStack)
{
    InstrThunk* returnPoint = instrp;
    pushFrame(block, env);
    getFrame()->setReturnPoint(returnPoint);
    instrp += ipOffset;
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

    inExceptionHandler_ = true;
    jumpKind_ = JumpKind::Exception;
    currentException_ = exception;
    Stack<ExceptionHandler*> handler(exceptionHandlers.back());
    exceptionHandlers.pop_back();
    while (frameIndex() != handler->frameIndex())
        popFrame();
    instrp = getFrame()->block()->startInstr() + handler->offset();
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

bool Interpreter::call(Traced<Value> targetValue,
                       const TracedVector<Value>& args,
                       MutableTraced<Value> resultOut)
{
    InstrThunk* oldInstrp = instrp;
    CallStatus status = setupCall(targetValue, args, resultOut);
    if (status == CallError)
        return false;

    if (status == CallFinished)
        return true;

    bool ok = run(resultOut);
    instrp = oldInstrp;
    return ok;
}

bool Interpreter::startCall(Traced<Value> targetValue, const TracedVector<Value>& args)
{
    InstrThunk* returnPoint = instrp;
    Stack<Value> result;
    CallStatus status = setupCall(targetValue, args, result);
    if (status == CallError) {
        pushStack(result);
        return false;
    }

    if (status == CallFinished) {
        pushStack(result);
        return true;
    }

    assert(status == CallStarted);
    getFrame()->setReturnPoint(returnPoint);
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

Interpreter::CallStatus Interpreter::setupCall(Traced<Value> targetValue,
                                               const TracedVector<Value>& args,
                                               MutableTraced<Value> resultOut)
{
    Stack<Object*> target(targetValue.toObject());
    if (target->is<Native>()) {
        Stack<Native*> native(target->as<Native>());
        if (!checkArguments(native, args, resultOut))
            return CallError;
        bool ok = native->call(args, resultOut);
        return ok ? CallFinished : CallError;
    } else if (target->is<Function>()) {
        Stack<Function*> function(target->as<Function>());
        if (!checkArguments(function, args, resultOut))
            return CallError;
        Stack<Env*> parentEnv(function->env());
        Stack<Block*> block(function->block());
        Stack<Env*> callEnv(gc.create<Env>(parentEnv, block));
        if (function->isGenerator()) {
            Stack<Value> none(None);
            callEnv->setAttr("%gen", none);
        }
        unsigned normalArgs = min(args.size(), function->maxNormalArgs());
        for (size_t i = 0; i < normalArgs; i++) {
            // todo: set by slot not name
            callEnv->setAttr(function->paramName(i), args[i]);
        }
        for (size_t i = args.size(); i < function->maxNormalArgs(); i++) {
            callEnv->setAttr(function->paramName(i),
                               function->paramDefault(i));
        }
        if (function->takesRest()) {
            Stack<Value> rest(Tuple::Empty);
            if (args.size() > function->maxNormalArgs()) {
                size_t count = args.size() - function->maxNormalArgs();
                size_t start = function->maxNormalArgs();
                TracedVector<Value> restArgs(args, start, count);
                rest = Tuple::get(restArgs);
            }
            callEnv->setAttr(function->paramName(function->restParam()),
                               rest);
        }
        if (function->isGenerator()) {
            Stack<Value> iter(
                gc.create<GeneratorIter>(function, callEnv));
            callEnv->setAttr("%gen", iter);
            resultOut = iter;
            return CallFinished;
        }
        pushFrame(block, callEnv);
        return CallStarted;
    } else if (target->is<Class>()) {
        // todo: this is messy and needs refactoring
        Stack<Class*> cls(target->as<Class>());
        Stack<Value> func;
        RootVector<Value> funcArgs(args.size() + 1);
        for (unsigned i = 0; i < args.size(); i++)
            funcArgs[i + 1] = args[i];
        if (target->maybeGetAttr(Name::__new__, func) && !func.isNone()) {
            // Create a new instance by calling static __new__ method
            funcArgs[0] = Value(cls);
            if (!call(func, funcArgs, resultOut))
                return CallError;
        } else {
            // Call the class' native constructor to create an instance
            string message = "cannot create '" + cls->name() + "' instances";
            return raiseTypeError(message, resultOut);
        }
        if (resultOut.isInstanceOf(cls)) {
            Stack<Value> initFunc;
            if (target->maybeGetAttr(Name::__init__, initFunc)) {
                Stack<Value> initResult;
                RootVector<Value> initArgs;
                initArgs.push_back(resultOut);
                for (unsigned i = 0; i < args.size(); i++)
                    initArgs.push_back(args[i]);
                if (!call(initFunc, initArgs, initResult))
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
        RootVector<Value> callArgs(args.size() + 1);
        callArgs[0] = method->object();
        for (size_t i = 0; i < args.size(); i++)
            callArgs[i] = args[i];
        Stack<Value> callable(method->callable());
        return setupCall(callable, callArgs, resultOut);
    } else {
        // todo: this is untested
        Stack<Value> callHook;
        if (!getAttr(targetValue, Name::__call__, callHook)) {
            return raiseTypeError(
                "object is not callable:" + repr(target), resultOut);
        }

        return setupCall(callHook, args, resultOut);
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
