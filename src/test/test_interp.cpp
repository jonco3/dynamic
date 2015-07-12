#include "src/interp.h"

#include "src/block.h"
#include "src/compiler.h"
#include "src/exception.h"
#include "src/instr.h"
#include "src/repr.h"
#include "src/test.h"

#include "test_interp.h"

void testInterp(const string& input, const string& expected)
{
    Stack<Block*> block;
    CompileModule(input, None, block);
    Stack<Value> result;
    bool ok = interp.exec(block, result);
    if (!ok)
        cerr << "Error: " << result.asObject()->as<Exception>()->message() << endl;
    testTrue(ok);
    testEqual(repr(result.get()), expected);
}

void testException(const string& input, const string& expected)
{
    Stack<Block*> block;
    CompileModule(input, None, block);
    Stack<Value> result;
    testExpectingException = true;
    bool ok = interp.exec(block, result);
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
    Stack<Block*> block;
    CompileModule(input, None, block);

    InstrThunk* instrp = block->findInstr(Instr_Lambda);
    assert(instrp);
    InstrLambda* lambda = instrp->data->as<InstrLambda>();
    instrp = lambda->block()->findInstr(initial);
    assert(instrp);

    Stack<Value> result;
    bool ok = interp.exec(block, result);
    if (!ok)
        cerr << "Error: " << result.asObject()->as<Exception>()->message() << endl;
    testTrue(ok);
    testEqual(repr(result.get()), expected);

    testEqual(instrp->data->type(), replacement);
}

template <typename T1, typename T2>
void testReplacement(const string& input,
                     const string& expected,
                     InstrType type,
                     InstrFunc<T1> initial,
                     InstrFunc<T2> replacement)
{
    Stack<Block*> block;
    CompileModule(input, None, block);

    InstrThunk* instrp = block->findInstr(Instr_Lambda);
    assert(instrp);
    InstrLambda* lambda = instrp->data->as<InstrLambda>();
    instrp = lambda->block()->findInstr(type);
    assert(instrp);
    testEqual(instrp->func, reinterpret_cast<InstrFuncBase>(initial));

    Stack<Value> result;
    bool ok = interp.exec(block, result);
    if (!ok)
        cerr << "Error: " << result.asObject()->as<Exception>()->message() << endl;
    testTrue(ok);
    testEqual(repr(result.get()), expected);

    testEqual(instrp->func, reinterpret_cast<InstrFuncBase>(replacement));
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

template <typename T1, typename T2, typename T3, typename T4>
void testReplacements(const string& defs,
                      const string& call1, const string& result1,
                      const string& call2, const string& result2,
                      InstrType type,
                      InstrFunc<T1> initial,
                      InstrFunc<T2> afterCall1,
                      InstrFunc<T3> afterCall2,
                      InstrFunc<T4> afterBoth)
{
    testReplacement(defs + "\n" + call1,
                    result1, type, initial, afterCall1);
    testReplacement(defs + "\n" + call2,
                    result2, type, initial, afterCall2);
    testReplacement(defs + "\n" + call1 + "\n" + call2,
                    result2, type, initial, afterBoth);
    testReplacement(defs + "\n" + call2 + "\n" + call1,
                    result1, type, initial, afterBoth);
}

testcase(interp)
{
    testEqual(interp.stackPos(), 0u);
    interp.pushStack(None);
    testEqual(interp.stackPos(), 1u);
    testEqual(interp.peekStack(0), Value(None));
    testEqual(interp.stackRef(0).get(), Value(None));

    {
        TracedVector<Value> slice = interp.stackSlice(1);
        testEqual(slice.size(), 1u);
        testEqual(slice[0].get(), Value(None));
    }

    testEqual(interp.popStack(), Value(None));
    testEqual(interp.stackPos(), 0u);

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
    //testInterp("foo = 0\n"
    //           "foo.bar = 1\n"
    //           "return foo + foo.bar", "1");
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
                     InstrGetMethod::execute,
                     InstrGetMethodInt::execute,
                     InstrGetMethod::fallback,
                     InstrGetMethod::fallback);

    testReplacements("def foo(x, y):\n"
                     "  return x + y",
                     "foo(1, 2)", "3",
                     "foo('a', 'b')", "'ab'",
                     Instr_BinaryOp,
                     InstrBinaryOp::execute,
                     InstrBinaryOpInt::execute,
                     InstrBinaryOp::fallback,
                     InstrBinaryOp::fallback);

    testExpectingException = true;
    testReplacements("x = None\n"
                     "def foo():\n"
                     "  y = 1\n"
                     "  if x == 0:\n"
                     "    del y\n"
                     "  try:\n"
                     "    return y\n"
                     "  except:\n"
                     "    return 0",
                     "x = 1; foo()", "1",
                     "x = 0; foo()", "0",
                     Instr_GetLocal,
                     InstrGetLocal::execute,
                     InstrGetLocal::execute,
                     InstrGetLocal::fallback,
                     InstrGetLocal::fallback);
    testExpectingException = false;

    testExpectingException = true;
    testReplacements("def foo(x):\n"
                     "  if x == 0:\n"
                     "    del x\n"
                     "  x = 1\n"
                     "  return x\n",
                     "foo(1)", "1",
                     "foo(0)", "1",
                     Instr_SetLocal,
                     InstrSetLocal::execute,
                     InstrSetLocal::execute,
                     InstrSetLocal::fallback,
                     InstrSetLocal::fallback);
    testExpectingException = false;

    testInterp("a, b = 1, 2\na", "1");
    testInterp("a, b = 1, 2\nb", "2");

    testException("raise Exception('an exception')", "an exception");

    testInterp("a = []\n"
               "for i in (1, 2, 3):\n"
               "  a.append(i + 1)\n"
               "a\n",
               "[2, 3, 4]");
}
