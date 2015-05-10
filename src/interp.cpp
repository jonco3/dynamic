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

//#define TRACE_INTERP

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

bool Interpreter::run(Root<Value>& resultOut)
{
#ifdef DEBUG
    unsigned initialPos = stackPos();
#endif
    while (instrp) {
        assert(getFrame()->block()->contains(instrp));
        Instr *instr = *instrp++;
#ifdef TRACE_INTERP
        cerr << "stack:";
        for (unsigned i = 0; i < stackPos(); ++i)
            cerr << " " << stack[i];
        cerr << endl;
        cerr << "frames:";
        for (unsigned i = 0; i < frames.size(); i++)
            cerr << " " << frames[i];
        cerr << endl << endl;
        cerr << "execute: " << repr(*instr);
        cerr << " line " << dec << currentPos().line << endl << endl;
#endif
        if (!instr->execute(*this)) {
            Root<Value> value(popStack());
#ifdef TRACE_INTERP
            cerr << "Exception: " << value << endl;
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
}

void Interpreter::popFrame()
{
    assert(!frames.empty());
    Frame* frame = frames.back();
    assert(exceptionHandlers.empty() ||
           exceptionHandlers.back()->frame() != frame);
    assert(frame->stackPos() <= stackPos());

    stack.resize(frame->stackPos());
    instrp = frame->returnPoint();
#ifdef TRACE_INTERP
    cout << "  return to " << (void*)instrp << endl;
#endif
    frames.pop_back();
}

void Interpreter::returnFromFrame(Value value)
{
    AutoAssertNoGC nogc;

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
        bool ok = startNextFinallySuite(JumpKind::LoopControl);
        assert(ok);
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
            resultOut = cls->nativeConstructor()(cls);
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

#ifdef BUILD_TESTS

#include "test.h"

void testInterp(const string& input, const string& expected)
{
#ifdef TRACE_INTERP
    cout << "testInterp: " << input << endl;
#endif
    Interpreter& interp = Interpreter::instance();
    assert(interp.stack.empty());
    assert(interp.frames.empty());
    assert(interp.exceptionHandlers.empty());
    assert(!interp.isHandlingException());
    assert(!interp.isHandlingDeferredReturn());
    Root<Block*> block;
    Block::buildModule(input, None, block);
    Root<Value> result;
    bool ok = Interpreter::exec(block, result);
    if (!ok)
        cerr << "Error: " << result.asObject()->as<Exception>()->message() << endl;
    testTrue(ok);
    testEqual(repr(result.get()), expected);
}

void testException(const string& input, const string& expected)
{
#ifdef TRACE_INTERP
    cout << "testException: " << input << endl;
#endif
    Root<Block*> block;
    Block::buildModule(input, None, block);
    Root<Value> result;
    testExpectingException = true;
    bool ok = Interpreter::exec(block, result);
    testExpectingException = false;
    if (ok) {
        cerr << "Expected exception but got: " << result << endl;
        abortTests();
    }
    Object *o = result.toObject();
    testTrue(o->isInstanceOf(Exception::ObjectClass));
    string message = o->as<Exception>()->fullMessage();
    if (message.find(expected) == string::npos) {
        cerr << "Expected message containing: " << expected << endl;
        cerr << "But got: " << message << endl;
        abortTests();
    }
}

void testReplacement(const string& input,
                     const string& expected,
                     InstrType initial,
                     InstrType replacement)
{
    Root<Block*> block;
    Block::buildModule(input, None, block);

    Instr** instrp = block->findInstr(Instr_Lambda);
    assert(instrp);
    InstrLambda* lambda = (*instrp)->as<InstrLambda>();
    instrp = lambda->block()->findInstr(initial);
    assert(instrp);

    Root<Value> result;
    bool ok = Interpreter::exec(block, result);
    if (!ok)
        cerr << "Error: " << result.asObject()->as<Exception>()->message() << endl;
    testTrue(ok);
    testEqual(repr(result.get()), expected);

    testEqual((*instrp)->type(), replacement);
}

void testReplacements(const string& defs,
                      const string& call1, const string& result1,
                      const string& call2, const string& result2,
                      InstrType initial,
                      InstrType afterCall1,
                      InstrType afterCall2,
                      InstrType afterBoth)
{
    testReplacement(defs + "\n" + call1,
                    result1, initial, afterCall1);
    testReplacement(defs + "\n" + call2,
                    result2, initial, afterCall2);
    testReplacement(defs + "\n" + call1 + "\n" + call2,
                    result2, initial, afterBoth);
    testReplacement(defs + "\n" + call2 + "\n" + call1,
                    result1, initial, afterBoth);
}

testcase(interp)
{
    Interpreter& i = Interpreter::instance();
    testEqual(i.stackPos(), 0u);
    i.pushStack(None);
    testEqual(i.stackPos(), 1u);
    testEqual(i.peekStack(0), Value(None));
    testEqual(i.stackRef(0).get(), Value(None));
    TracedVector<Value> slice = i.stackSlice(1);
    testEqual(slice.size(), 1u);
    testEqual(slice[0].get(), Value(None));
    testEqual(i.popStack(), Value(None));
    testEqual(i.stackPos(), 0u);

    testInterp("pass", "None");
    testInterp("return 3", "3");
    testInterp("return 2 + 2", "4");
    testInterp("return 2 ** 4 - 1", "15");
    testInterp("foo = 2 + 3\n"
               "return foo", "5");
    testInterp("foo = 3\n"
               "bar = 4\n"
               "return foo + bar", "7");
    // todo: should be: AttributeError: 'int' object has no attribute 'foo'
    testInterp("foo = 0\n"
               "foo.bar = 1\n"
               "return foo + foo.bar", "1");
    testInterp("foo = bar = 1\n"
               "return foo", "1");
    testInterp("return 1 | 8", "9");
    testInterp("return 3 ^ 5", "6");
    testInterp("return 3 & 5", "1");
    testInterp("return 2 > 3", "False");
    testInterp("return 3 > 3", "False");
    testInterp("return 3 > 2", "True");
    testInterp("return 2 == 3", "False");
    testInterp("return 3 == 3", "True");
    testInterp("return 3 == 2", "False");
    testInterp("return 1 or 2", "1");
    testInterp("return 0 or 2", "2");
    testInterp("return 1 and 2", "2");
    testInterp("return 0 and 2", "0");
    testInterp("return -2", "-2");
    testInterp("return --2", "2");
    testInterp("return -2 + 1", "-1");
    testInterp("return 2 - -1", "3");
    testInterp("return 1 if 2 < 3 else 0", "1");
    testInterp("return 1 if 2 > 3 else 2 + 2", "4");
    testInterp("return (lambda: 2 + 2)()", "4");
    testInterp("return (lambda x: x + 1)(1)", "2");
    testInterp("x = 0\nreturn (lambda x: x + 1)(1)", "2");
    testInterp("x = 0\nreturn (lambda y: x + 1)(1)", "1");
    testInterp("return not 1", "False");
    testInterp("return not 0", "True");

    testInterp("if 1:\n"
               "  return 2\n", "2");
    testInterp("if 0:\n"
               "  return 2\n"
               "else:\n"
               "  return 3\n",
               "3");
    testInterp("x = 1\n"
               "if x == 0:\n"
               "  return 4\n"
               "elif x == 1:\n"
               "  return 5\n"
               "else:\n"
               "  return 6\n",
               "5");

    testInterp("x = 0\n"
               "y = 3\n"
               "while y > 0:\n"
               "  x = x + y\n"
               "  y = y - 1\n"
               "return x\n",
               "6");

    testInterp("def foo():\n"
               "  return 1\n"
               "foo()\n",
               "1");

    testInterp("def foo():\n"
               "  1\n"
               "foo()\n",
               "None");

    testInterp("def foo(x):\n"
               "  return x\n"
               "foo(3)\n",
               "3");

    testInterp("def foo(x = 2):\n"
               "  return x\n"
               "foo()\n",
               "2");

    testInterp("def foo(x = 1, y = 2):\n"
               "  return (x, y)\n"
               "foo(3)\n",
               "(3, 2)");

    testInterp("def foo(x = 1, y = 2):\n"
               "  return (x, y)\n"
               "foo()\n",
               "(1, 2)");

    testInterp("def foo(*x):\n"
               "  return x\n"
               "foo()\n",
               "()");

    testInterp("def foo(*x):\n"
               "  return x\n"
               "foo(1, 2)\n",
               "(1, 2)");

    testInterp("def foo(x, *y):\n"
               "  return (x, y)\n"
               "foo(1, 2)\n",
               "(1, (2,))");

    testInterp("def foo(x = 1, *y):\n"
               "  return (x, y)\n"
               "foo()\n",
               "(1, ())");

    testInterp("class Foo:\n"
               "  a = 1\n"
               "Foo().a",
               "1");

    testInterp("assert True", "None");
    testInterp("assert 2 + 2, 'ok'\n", "None");
    testException("assert False", "AssertionError");
    testException("assert 0, 'bad'", "AssertionError: bad");

    testException("1()", "object is not callable");
    testException("1.foo", "object has no attribute 'foo'");

    testException("def foo():\n"
                  "  return 1\n"
                  "foo(1)\n",
                  "foo() takes 0 positional arguments but 1 was given");
    testException("def foo(x):\n"
                  "  return 1\n"
                  "foo(1, 2)\n",
                  "foo() takes 1 positional arguments but 2 were given");
    testException("def foo(a):\n"
                  "  return 1\n"
                  "foo()\n",
                  "foo() takes 1 positional arguments but 0 were given");

    testReplacements("def foo(x, y):\n"
                     "  return x.__add__(y)",
                     "foo(1, 2)", "3",
                     "foo('a', 'b')", "'ab'",
                     Instr_GetMethod,
                     Instr_GetMethodInt,
                     Instr_GetMethodFallback,
                     Instr_GetMethodFallback);

    testReplacements("def foo(x, y):\n"
                     "  return x + y",
                     "foo(1, 2)", "3",
                     "foo('a', 'b')", "'ab'",
                     Instr_BinaryOp,
                     Instr_BinaryOpInt,
                     Instr_BinaryOpFallback,
                     Instr_BinaryOpFallback);

    testInterp("a, b = 1, 2\na", "1");
    testInterp("a, b = 1, 2\nb", "2");

    testException("raise Exception('an exception')", "an exception");

    testInterp("a = []\n"
               "for i in (1, 2, 3):\n"
               "  a.append(i + 1)\n"
               "a\n",
               "[2, 3, 4]");
}

#endif
