#include "block.h"
#include "callable.h"
#include "exception.h"
#include "frame.h"
#include "input.h"
#include "instr.h"
#include "interp.h"
#include "repr.h"

#include "value-inl.h"

//#define TRACE_INTERP

Interpreter* Interpreter::instance_ = nullptr;

/* static */ void Interpreter::init()
{
    assert(!instance_);
    instance_ = new Interpreter;
}

/* static */ bool Interpreter::exec(Traced<Block*> block, Value& valueOut)
{
    assert(instance_);
    return instance_->interpret(block, valueOut);
}

Interpreter::Interpreter()
  : instrp(nullptr), pos(0)
{}

bool Interpreter::interpret(Traced<Block*> block, Value& valueOut)
{
    assert(stackPos() == 0);
    Root<Frame*> frame(gc::create<Frame>(block));
    pushFrame(frame);
    return run(valueOut);
}

bool Interpreter::run(Value& valueOut)
{
    while (instrp) {
        assert(getFrame()->block()->contains(instrp));
        Instr *instr = *instrp++;
#ifdef TRACE_INTERP
        cerr << "stack:";
        for (unsigned i = 0; i < pos; ++i)
            cerr << " " << stack[i];
        cerr << endl;
        cerr << "  execute " << repr(instr) << endl;
#endif
        if (!instr->execute(*this)) {
            valueOut = popStack();
#ifdef TRACE_INTERP
            cerr << "Error: " << valueOut << endl;
#endif
            assert(valueOut.asObject()->is<Exception>());
            while (instrp)
                popFrame();
            return false;
        }
    }

    assert(stackPos() == 1);
    valueOut = popStack();
    return true;
}

Frame* Interpreter::newFrame(Traced<Function*> function)
{
    Root<Block*> block(function->block());
    return gc::create<Frame>(block);
}

void Interpreter::pushFrame(Traced<Frame*> frame)
{
    frame->setStackPos(pos);
    instrp = frame->block()->startInstr();
    frames.push_back(frame);
}

void Interpreter::popFrame()
{
    assert(!frames.empty());
    Frame* frame = frames.back();
    assert(frame->stackPos() <= pos);
    pos = frame->stackPos();
    stack.resize(pos);
    instrp = frame->returnPoint();
#ifdef TRACE_INTERP
    cout << "  return to " << (void*)instrp << endl;
#endif
    frames.pop_back();
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

bool Interpreter::call(Traced<Value> targetValue, const TracedVector<Value>& args,
                       Root<Value>& resultOut)
{
    Instr** oldInstrp = instrp;
    CallStatus status = setupCall(targetValue, args, resultOut);
    if (status == CallError)
        return false;

    if (status == CallFinished)
        return true;

    Value result; // todo: root out param
    bool ok = run(result);
    resultOut = result;
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

Interpreter::CallStatus Interpreter::setupCall(Traced<Value> targetValue,
                                               const TracedVector<Value>& args,
                                               Root<Value>& resultOut)
{
    Root<Object*> target(targetValue.toObject());
    if (target->is<Native>()) {
        Root<Native*> native(target->as<Native>());
        if (native->requiredArgs() != args.size())
            return raise("TypeError", "Wrong number of arguments", resultOut);
        bool ok = native->call(args, resultOut);
        return ok ? CallFinished : CallError;
    } else if (target->is<Function>()) {
        Root<Function*> function(target->as<Function>());
        if (function->requiredArgs() != args.size())
            return raise("TypeError", "Wrong number of arguments", resultOut);
        Root<Frame*> callFrame(newFrame(function));
        for (int i = args.size() - 1; i >= 0; --i)
            callFrame->setAttr(function->paramName(i), args[i]);
        pushFrame(callFrame);
        return CallStarted;
    } else if (target->is<Class>()) {
        Root<Class*> cls(target->as<Class>());
        Root<Object*> instance(gc::create<Object>(cls));  // todo: maybe call __new__
        Value attr; // todo: root
        if (!target->maybeGetAttr("__init__", attr)) {
            if (args.size() != 0)
                return raise("TypeError", "this constructor takes no arguments", resultOut);
            resultOut = instance;
            return CallFinished;
        }
        Root<Value> initFunc(attr);

        RootVector<Value> initArgs;
        initArgs.push_back(Value(instance));
        for (unsigned i = 0; i < args.size(); i++)
            initArgs.push_back(args[i]);
        if (!call(initFunc, initArgs, resultOut))
            return CallError;

        if (resultOut.get().toObject() != None)
            return raise("TypeError", "__init__() should return None", resultOut);

        resultOut = instance;
        return CallFinished;
    } else {
        return raise("TypeError", "object is not callable:" + repr(target), resultOut);
    }
}

Interpreter::CallStatus Interpreter::raise(string className, string message,
                                           Root<Value>& resultOut)
{
    resultOut = gc::create<Exception>(className, message);
    return CallError;
}

void Interpreter::replaceInstr(Instr* instr, Instr* prev)
{
    assert(instrp[-1] == prev);
    instrp[-1] = instr;
}

#ifdef BUILD_TESTS

#include "test.h"

void testInterp(const string& input, const string& expected)
{
#ifdef TRACE_INTERP
    cout << "testInterp: " << input << endl;
#endif
    Root<Block*> block;
    Block::buildModule(input, Object::Null, block);
    Value result;
    bool ok = Interpreter::exec(block, result);
    if (!ok)
        cerr << "Error: " << result.asObject()->as<Exception>()->message() << endl;
    testTrue(ok);
    testEqual(repr(result), expected);
}

void testException(const string& input, const string& expected)
{
#ifdef TRACE_INTERP
    cout << "testException: " << input << endl;
#endif
    Root<Block*> block;
    Block::buildModule(input, Object::Null, block);
    Value result;
    bool ok = Interpreter::exec(block, result);
    if (ok) {
        cerr << "Expected exception but got: " << result << endl;
        abortTests();
    }
    Object *o = result.toObject();
    testTrue(o->is<Exception>());
    string message = o->as<Exception>()->fullMessage();
    if (message.find(expected) == string::npos) {
        cerr << "Expected message containing: " << expected << endl;
        cerr << "But got: " << message << endl;
        abortTests();
    }
}

void testReplacement(const string& input, const string& expected, InstrType initial,
                     InstrType replacement)
{
    Root<Block*> block;
    Block::buildModule(input, Object::Null, block);

    Instr** instrp = block->findInstr(Instr_Lambda);
    assert(instrp);
    InstrLambda* lambda = (*instrp)->as<InstrLambda>();
    instrp = lambda->block()->findInstr(initial);
    assert(instrp);

    Value result;
    bool ok = Interpreter::exec(block, result);
    if (!ok)
        cerr << "Error: " << result.asObject()->as<Exception>()->message() << endl;
    testTrue(ok);
    testEqual(repr(result), expected);

    testEqual((*instrp)->type(), replacement);
}

testcase(interp)
{
    Interpreter& i = Interpreter::instance();
    testEqual(i.stackPos(), 0);
    i.pushStack(None);
    testEqual(i.stackPos(), 1);
    Root<Value> v(None);
    testEqual(i.peekStack(0), v);
    testEqual(i.stackRef(0).get(), v);
    TracedVector<Value> slice = i.stackSlice(0, 1);
    testEqual(slice.size(), 1);
    testEqual(slice[0].get(), v.get());
    testEqual(i.popStack(), v);
    testEqual(i.stackPos(), 0);

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
    // todo: fails to parse with: Illegal LHS for assignment
    //testInterp("foo = bar = 1\n"
    //           "return foo", "1");
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
                  "Wrong number of arguments");
    testException("def foo(a):\n"
                  "  return 1\n"
                  "foo()\n",
                  "Wrong number of arguments");

    testReplacement("def foo(x, y):\n"
                    "  return x + y\n"
                    "foo('a', 'b')\n",
                    "'ab'",
                    Instr_GetMethod,
                    Instr_GetMethodFallback);

    testReplacement("def foo(x, y):\n"
                    "  return x + y\n"
                    "foo(1, 2)\n",
                    "3",
                    Instr_GetMethod,
                    Instr_GetMethodInt);

    testReplacement("def foo(x, y):\n"
                    "  return x + y\n"
                    "foo(1, 2)\n"
                    "foo('a', 'b')\n",
                    "'ab'",
                    Instr_GetMethod,
                    Instr_GetMethodFallback);

    testReplacement("def foo(x, y):\n"
                    "  return x + y\n"
                    "foo('a', 'b')\n"
                    "foo(1, 2)\n",
                    "3",
                    Instr_GetMethod,
                    Instr_GetMethodFallback);

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
