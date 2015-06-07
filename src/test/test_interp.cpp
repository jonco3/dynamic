#include "src/interp.h"

#include "src/block.h"
#include "src/exception.h"
#include "src/instr.h"
#include "src/repr.h"
#include "src/test.h"

#include "test_interp.h"

void testInterp(const string& input, const string& expected)
{
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