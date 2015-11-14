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
    bool ok = interp->exec(block, result);
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
    bool ok = interp->exec(block, result);
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
                     InstrCode initial,
                     InstrCode replacement,
                     InstrCode next1 = InstrCodeCount,
                     InstrCode next2 = InstrCodeCount)
{
    Stack<Block*> block;
    CompileModule(input, None, block);

    InstrThunk* instrp = block->findInstr(Instr_Lambda);
    assert(instrp);
    Instr* instr = instrp->data;
    assert(instr->code() == Instr_Lambda);
    LambdaInstr* lambda = static_cast<LambdaInstr*>(instr);
    instrp = lambda->block()->findInstr(initial);
    assert(instrp);

    Stack<Value> result;
    bool ok = interp->exec(block, result);
    if (!ok)
        cerr << "Error: " << result.asObject()->as<Exception>()->message() << endl;
    testTrue(ok);
    testEqual(repr(result.get()), expected);

    instr = instrp->data;
    testEqual(instrName(instr->code()), instrName(replacement));

    if (next1 < InstrCodeCount) {
        instr = getNextInstr(instr);
        assert(instr);
        testEqual(instrName(instr->code()), instrName(next1));

        if (next2 < InstrCodeCount) {
            instr = getNextInstr(instr);
            assert(instr);
            testEqual(instrName(instr->code()), instrName(next2));
        }
    }
}

void testReplacements(const string& defs,
                      const string& call1, const string& result1,
                      const string& call2, const string& result2,
                      InstrCode initial,
                      InstrCode afterCall1,
                      InstrCode afterCall2,
                      InstrCode afterCall1Then2,
                      InstrCode afterCall2Then1)
{
    testReplacement(defs + "\n" + call1, result1, initial, afterCall1);
    testReplacement(defs + "\n" + call2, result2, initial, afterCall2);
    testReplacement(defs + "\n" + call1 + "\n" + call2,
                  result2, initial, afterCall1Then2);
    testReplacement(defs + "\n" + call2 + "\n" + call1,
                  result1, initial, afterCall2Then1);
}

void testReplacements(const string& defs,
                      const string& call1, const string& result1,
                      const string& call2, const string& result2,
                      InstrCode initial,
                      InstrCode afterCall1,
                      InstrCode afterCall2,
                      InstrCode afterBoth)
{
    testReplacements(defs, call1, result1, call2, result2,
                     initial, afterCall1, afterCall2, afterBoth, afterBoth);
}

void testStubs(const string& defs,
               const string& call1, const string& result1,
               const string& call2, const string& result2,
               InstrCode initial,
               InstrCode afterCall1,
               InstrCode afterCall2)
{
    testReplacement(defs + "\n" + call1, result1, initial, afterCall1, initial);
    testReplacement(defs + "\n" + call2, result2, initial, afterCall2, initial);
    testReplacement(defs + "\n" + call1 + "\n" + call2,
                    result2, initial, afterCall2, afterCall1, initial);
    testReplacement(defs + "\n" + call2 + "\n" + call1,
                    result1, initial, afterCall1, afterCall2, initial);
}

testcase(interp)
{
    testInterp("pass", "None");
    testInterp("3", "3");
    testInterp("2 + 2", "4");
    testInterp("2 ** 4 - 1", "15");
    testInterp("foo = 2 + 3\n"
               "foo", "5");
    testInterp("foo = 3\n"
               "bar = 4\n"
               "foo + bar", "7");
    // todo: should be: AttributeError: 'int' object has no attribute 'foo'
    //testInterp("foo = 0\n"
    //           "foo.bar = 1\n"
    //           "foo + foo.bar", "1");
    testInterp("foo = bar = 1\n"
               "foo", "1");
    testInterp("1 | 8", "9");
    testInterp("3 ^ 5", "6");
    testInterp("3 & 5", "1");
    testInterp("2 > 3", "False");
    testInterp("3 > 3", "False");
    testInterp("3 > 2", "True");
    testInterp("2 == 3", "False");
    testInterp("3 == 3", "True");
    testInterp("3 == 2", "False");
    testInterp("1 or 2", "1");
    testInterp("0 or 2", "2");
    testInterp("1 and 2", "2");
    testInterp("0 and 2", "0");
    testInterp("-2", "-2");
    testInterp("--2", "2");
    testInterp("-2 + 1", "-1");
    testInterp("2 - -1", "3");
    testInterp("1 if 2 < 3 else 0", "1");
    testInterp("1 if 2 > 3 else 2 + 2", "4");
    testInterp("(lambda: 2 + 2)()", "4");
    testInterp("(lambda x: x + 1)(1)", "2");
    testInterp("x = 0\n(lambda x: x + 1)(1)", "2");
    testInterp("x = 0\n(lambda y: x + 1)(1)", "1");
    testInterp("not 1", "False");
    testInterp("not 0", "True");

    testInterp("if 1:\n"
               "  2\n", "2");
    testInterp("if 0:\n"
               "  2\n"
               "else:\n"
               "  3\n",
               "3");
    testInterp("x = 1\n"
               "if x == 0:\n"
               "  4\n"
               "elif x == 1:\n"
               "  5\n"
               "else:\n"
               "  6\n",
               "5");

    testInterp("x = 0\n"
               "y = 3\n"
               "while y > 0:\n"
               "  x = x + y\n"
               "  y = y - 1\n"
               "x\n",
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

    testStubs("def foo(x, y):\n"
                     "  return x.__add__(y)",
                     "foo(1, 2)", "3",
                     "foo('a', 'b')", "'ab'",
                     Instr_GetMethod,
                     Instr_GetMethodBuiltin,
                     Instr_GetMethodBuiltin);

    testStubs("def foo(x, y):\n"
              "  return x + y",
              "foo(1, 2)", "3",
              "foo('a', 'b')", "'ab'",
              Instr_BinaryOp,
              Instr_BinaryOpInt_Add,
              Instr_BinaryOpBuiltin);

    testStubs("def foo(x, y):\n"
              "  return x - y",
              "foo(2.75, 0.5)", "2.25",
              "foo(5, 1)", "4",
              Instr_BinaryOp,
              Instr_BinaryOpFloat_Sub,
              Instr_BinaryOpInt_Sub);

    testStubs("def foo(x, y):\n"
              "  return x > y",
              "foo(2.75, 0.5)", "True",
              "foo(1, 2)", "False",
              Instr_CompareOp,
              Instr_CompareOpFloat_GT,
              Instr_CompareOpInt_GT);

    testStubs("def foo(x, y):\n"
              "  x += y\n"
              "  return x",
              "foo(1, 2)", "3",
              "foo('a', 'b')", "'ab'",
              Instr_AugAssignUpdate,
              Instr_AugAssignUpdateInt_Add,
              Instr_AugAssignUpdateBuiltin);

    testStubs("def foo(x, y):\n"
              "  x *= y\n"
              "  return x",
              "foo(2.0, 1.25)", "2.5",
              "foo(2, 2)", "4",
              Instr_AugAssignUpdate,
              Instr_AugAssignUpdateFloat_Mul,
              Instr_AugAssignUpdateInt_Mul);

    testReplacements("g = 1\n"
                     "def foo():\n"
                     "  return g",
                     "foo()", "1",
                     "a = 1; foo()", "1",
                     Instr_GetGlobal,
                     Instr_GetGlobalSlot,
                     Instr_GetGlobalSlot,
                     Instr_GetGlobalSlot);

    testReplacements("g = 1\n"
                     "f = 0\n"
                     "def foo():\n"
                     "  return g",
                     "foo()", "1",
                     "del f; foo()", "1",
                     Instr_GetGlobal,
                     Instr_GetGlobalSlot,
                     Instr_GetGlobalSlot,
                     Instr_GetGlobalSlot);

    testReplacements("def foo():\n"
                     "  return len([])",
                     "foo()", "0",
                     "a = 0; foo()", "0",
                     Instr_GetGlobal,
                     Instr_GetBuiltinsSlot,
                     Instr_GetBuiltinsSlot,
                     Instr_GetBuiltinsSlot);

    // todo: add test for GetBuiltinsSlot where we modify __builtins__

    testReplacements("g = 1\n"
                     "def foo():\n"
                     "  global g\n"
                     "  g = 2\n"
                     "  return 1",
                     "foo()", "1",
                     "a = 1; foo()", "1",
                     Instr_SetGlobal,
                     Instr_SetGlobalSlot,
                     Instr_SetGlobalSlot,
                     Instr_SetGlobalSlot);

    testExpectingException = true;
    testInterp("x = None\n"
               "def foo():\n"
               "  y = 1\n"
               "  if x == 0:\n"
               "    del y\n"
               "  try:\n"
               "    return y\n"
               "  except:\n"
               "    return 0\n"
               "x = 1; foo()",
               "1");
    testInterp("x = None\n"
               "def foo():\n"
               "  y = 1\n"
               "  if x == 0:\n"
               "    del y\n"
               "  try:\n"
               "    return y\n"
               "  except:\n"
               "    return 0\n"
               "x = 0; foo()",
               "0");
    testExpectingException = false;

    testExpectingException = true;
    testInterp("def foo(x):\n"
               "  if x == 0:\n"
               "    del x\n"
               "  x = 1\n"
               "  return x\n"
               "foo(1)",
               "1");
    testInterp("def foo(x):\n"
               "  if x == 0:\n"
               "    del x\n"
               "  x = 1\n"
               "  return x\n"
               "foo(0)",
               "1");
    testExpectingException = false;

    testInterp("a, b = 1, 2\na", "1");
    testInterp("a, b = 1, 2\nb", "2");
    testException("a, b = 1,", "too few values");
    testException("a, b = 1, 2, 3", "too many values");

    testException("raise Exception('an exception')", "an exception");

    testInterp("a = []\n"
               "for i in (1, 2, 3):\n"
               "  a.append(i + 1)\n"
               "a\n",
               "[2, 3, 4]");
}
