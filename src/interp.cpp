#include "block.h"
#include "callable.h"
#include "exception.h"
#include "frame.h"
#include "generator.h"
#include "input.h"
#include "instr.h"
#include "interp.h"
#include "repr.h"

#include "value-inl.h"

#ifdef DEBUG
bool logFrames = false;
bool logExecution = false;
#endif

ExceptionHandler::ExceptionHandler(Type type, Traced<Frame*> frame,
                                   unsigned offset)
  : type_(type), frame_(frame), offset_(offset)
{}

void ExceptionHandler::traceChildren(Tracer& t)
{
    gc.trace(t, &frame_);
}

Interpreter* Interpreter::instance_ = nullptr;

/* static */ void Interpreter::init()
{
    assert(!instance_);
    instance_ = new Interpreter;
}

/* static */ bool Interpreter::exec(Traced<Block*> block, Root<Value>& resultOut)
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

bool Interpreter::interpret(Traced<Block*> block, Root<Value>& resultOut)
{
    assert(stackPos() == 0);
    Root<Frame*> frame(gc.create<Frame>(block));
    pushFrame(frame);
    return run(resultOut);
}

#ifdef DEBUG
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

bool Interpreter::run(Root<Value>& resultOut)
{
#ifdef DEBUG
    unsigned initialPos = stackPos();
#endif
    while (instrp) {
        assert(getFrame()->block()->contains(instrp));
        Instr *instr = *instrp++;
#ifdef DEBUG
        if (logExecution) {
            logStart();
            cout << *instr << " at line " << dec << currentPos().line << endl;
        }
#endif
        if (!instr->execute(*this)) {
            Root<Value> value(popStack());
#ifdef DEBUG
        if (logExecution) {
            logStart(1);
            cout << "exception " << value << endl;
        }
#endif
            assert(value.isInstanceOf(Exception::ObjectClass));
            Root<Exception*> exception(value.toObject()->as<Exception>());
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

Frame* Interpreter::newFrame(Traced<Function*> function)
{
    Root<Block*> block(function->block());
    return gc.create<Frame>(block);
}

void Interpreter::pushFrame(Traced<Frame*> frame)
{
    frame->setStackPos(stackPos());
    instrp = frame->block()->startInstr();
    frames.push_back(frame);
    if (logFrames) {
        TokenPos pos = frame->block()->getPos(instrp);
        logStart(-1);
        cout << "> frame " << pos << endl;
    }
}

void Interpreter::popFrame()
{
    assert(!frames.empty());
    Frame* frame = frames.back();
    if (logFrames) {
        TokenPos pos = frame->block()->getPos(instrp - 1);
        logStart(-1);
        cout << "< frame " << pos << endl;
    }
    assert(exceptionHandlers.empty() ||
           exceptionHandlers.back()->frame() != frame);
    assert(frame->stackPos() <= stackPos());

    stack.resize(frame->stackPos());
    instrp = frame->returnPoint();
    frames.pop_back();
}

#ifdef DEBUG
unsigned Interpreter::frameStackDepth()
{
    assert(!frames.empty());
    Frame* frame = frames.back();
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

#ifdef DEBUG
#define alwaysTrue(x) assert(x)
#else
#define alwaysTrue(x) x
#endif

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

void Interpreter::resumeGenerator(Traced<Frame*> frame,
                                  unsigned ipOffset,
                                  vector<Value>& savedStack)
{
    frame->setReturnPoint(instrp);
    pushFrame(frame);
    instrp += ipOffset;
    for (auto i = savedStack.begin(); i != savedStack.end(); i++)
        pushStack(*i);
    savedStack.resize(0);
}

unsigned Interpreter::suspendGenerator(vector<Value>& savedStack)
{
    Frame* frame = frames.back();
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
    Instr** start = getFrame()->block()->startInstr();
    assert(instrp >= start + 1);
    return instrp - start - 1;
}

void Interpreter::pushExceptionHandler(ExceptionHandler::Type type,
                                       unsigned offset)
{
    assert(offset);
    Root<Frame*> frame(getFrame());
    // todo: Why do we store instruct offsets rather than indices?
    Root<ExceptionHandler*> handler(
        gc.create<ExceptionHandler>(type, frame, offset + currentOffset()));
    exceptionHandlers.push_back(handler);
}

void Interpreter::popExceptionHandler(ExceptionHandler::Type type)
{
#ifdef DEBUG
    ExceptionHandler* handler = exceptionHandlers.back();
    assert(handler->frame() == getFrame());
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
    ExceptionHandler* handler = exceptionHandlers.back();
    exceptionHandlers.pop_back();
    while (getFrame() != handler->frame())
        popFrame();
    instrp = getFrame()->block()->startInstr() + handler->offset();
    return true;
}

bool Interpreter::startNextFinallySuite(JumpKind jumpKind)
{
    assert(jumpKind == JumpKind::Return || jumpKind == JumpKind::LoopControl);
    Frame* frame = getFrame();
    while (ExceptionHandler* handler = currentExceptionHandler()) {
        if (handler->frame() != frame)
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
        Root<Exception*> exception(currentException_);
        finishHandlingException();
        if (!startExceptionHandler(exception)) {
            pushStack(exception);
            return false;
        }
        break;
      }

      case JumpKind::Return: {
        Root<Value> value(deferredReturnValue_);
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
    if (handler->frame() != getFrame())
        return nullptr;

    return handler;
}

Frame* Interpreter::getFrame(unsigned reverseIndex)
{
    assert(reverseIndex < frames.size());
    return frames[frames.size() - 1 - reverseIndex];
}

void Interpreter::branch(int offset)
{
    Instr** target = instrp + offset - 1;
    assert(frames.back()->block()->contains(target));
    instrp = target;
}

TokenPos Interpreter::currentPos()
{
    assert(instrp);
    assert(!frames.empty());
    return frames.back()->block()->getPos(instrp - 1);
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
                       Root<Value>& resultOut)
{
    Instr** oldInstrp = instrp;
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
    Instr** returnPoint = instrp;
    Root<Value> result;
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
                                 Root<Value>& resultOut)
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
                                               Root<Value>& resultOut)
{
    Root<Object*> target(targetValue.toObject());
    if (target->is<Native>()) {
        Root<Native*> native(target->as<Native>());
        if (!checkArguments(native, args, resultOut))
            return CallError;
        bool ok = native->call(args, resultOut);
        return ok ? CallFinished : CallError;
    } else if (target->is<Function>()) {
        Root<Function*> function(target->as<Function>());
        if (!checkArguments(function, args, resultOut))
            return CallError;
        Root<Frame*> callFrame(newFrame(function));
        if (function->isGenerator()) {
            Root<Value> none(None);
            callFrame->setAttr("%gen", none);
        }
        unsigned normalArgs = min(args.size(), function->maxNormalArgs());
        for (size_t i = 0; i < normalArgs; i++) {
            // todo: set by slot not name
            callFrame->setAttr(function->paramName(i), args[i]);
        }
        for (size_t i = args.size(); i < function->maxNormalArgs(); i++) {
            callFrame->setAttr(function->paramName(i),
                               function->paramDefault(i));
        }
        if (function->takesRest()) {
            Root<Value> rest(Tuple::Empty);
            if (args.size() > function->maxNormalArgs()) {
                size_t count = args.size() - function->maxNormalArgs();
                size_t start = function->maxNormalArgs();
                TracedVector<Value> restArgs(args, start, count);
                rest = Tuple::get(restArgs);
            }
            callFrame->setAttr(function->paramName(function->restParam()),
                               rest);
        }
        if (function->isGenerator()) {
            Root<Value> iter(
                gc.create<GeneratorIter>(function, callFrame));
            callFrame->setAttr("%gen", iter);
            resultOut = iter;
            return CallFinished;
        }
        pushFrame(callFrame);
        return CallStarted;
    } else if (target->is<Class>()) {
        // todo: this is messy and needs refactoring
        Root<Class*> cls(target->as<Class>());
        Root<Value> func;
        RootVector<Value> funcArgs(args.size() + 1);
        for (unsigned i = 0; i < args.size(); i++)
            funcArgs[i + 1] = args[i];
        if (target->maybeGetAttr("__new__", func)) {
            // Create a new instance by calling static __new__ method
            funcArgs[0] = Value(cls);
            if (!call(func, funcArgs, resultOut))
                return CallError;
        } else {
            // Call the class' native constructor to create an instance
            Class::NativeConstructor nc = cls->nativeConstructor();
            if (!nc) {
                string message =
                    "cannot create '" + cls->name() + "' instances";
                return raiseTypeError(message, resultOut);
            }
            resultOut = nc(cls);
        }
        if (resultOut.isInstanceOf(cls)) {
            Root<Value> initFunc;
            if (target->maybeGetAttr("__init__", initFunc)) {
                Root<Value> initResult;
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
    } else {
        return raiseTypeError(
            "object is not callable:" + repr(target), resultOut);
    }
}

Interpreter::CallStatus Interpreter::raiseTypeError(string message,
                                                    Root<Value>& resultOut)
{
    resultOut = gc.create<TypeError>(message);
    return CallError;
}

void Interpreter::replaceInstr(Instr* current, Instr* newInstr)
{
    assert(instrp[-1] == current);
    instrp[-1] = newInstr;
}

bool Interpreter::replaceInstrAndRestart(Instr* current, Instr* newInstr)
{
    replaceInstr(current, newInstr);
    return newInstr->execute(*this);
}
