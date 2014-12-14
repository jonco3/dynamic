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

Interpreter::Interpreter()
  : instrp(nullptr), pos(0)
{}

bool Interpreter::interpret(Traced<Block*> block, Value& valueOut)
{
    Root<Frame*> frame(new Frame(block));
    pushFrame(frame);

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
    return new Frame(block, instrp);
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
    stack.resize(frame->stackPos());
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


bool Interpreter::raise(string message) {
    pushStack(new Exception(message));
    return false;
}

bool Interpreter::startCall(Traced<Value> targetValue, const TracedVector<Value>& args)
{
    Root<Object*> target(targetValue.toObject());
    if (target->is<Native>()) {
        Root<Native*> native(target->as<Native>());
        if (native->requiredArgs() != args.size())
            return raise("Wrong number of arguments");
        Root<Value> result;
        bool ok = native->call(args, result);
        pushStack(result);
        return ok;
    } else if (target->is<Function>()) {
        Root<Function*> function(target->as<Function>());
        if (function->requiredArgs() != args.size())
            return raise("Wrong number of arguments");
        Root<Frame*> callFrame(newFrame(function));
        for (int i = args.size() - 1; i >= 0; --i)
            callFrame->setAttr(function->paramName(i), args[i]);
        pushFrame(callFrame);
        return true;
    } else {
        return raise("TypeError: object is not callable");
    }
}

#ifdef BUILD_TESTS

#include "test.h"

void testInterp(const string& input, const string& expected)
{
#ifdef TRACE_INTERP
    cout << "testInterp: " << input << endl;
#endif
    Root<Block*> block(Block::buildModule(input, Object::Null));
    Interpreter interp;
    Value result;
    bool ok = interp.interpret(block, result);
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
    Root<Block*> block(Block::buildModule(input, Object::Null));
    Interpreter interp;
    Value result;
    bool ok = interp.interpret(block, result);
    if (ok) {
        cerr << "Expected exception but got: " << result << endl;
        abortTests();
    }
    Object *o = result.toObject();
    testTrue(o->is<Exception>());
    string message = o->as<Exception>()->message();
    if (message.find(expected) == string::npos) {
        cerr << "Expected message containing: " << expected << endl;
        cerr << "But got: " << message << endl;
        abortTests();
    }
}

testcase(interp)
{
    Interpreter i;
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
}

#endif
