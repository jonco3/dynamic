#include "src/block.h"

#include "src/repr.h"
#include "src/singletons.h"
#include "src/test.h"
#include "src/utils.h"
#include "src/value-inl.h"

static void testBuildModule(const string& input, const string& expected)
{
#ifdef DEBUG
    AutoSetAndRestore asar(assertStackDepth, false);
#endif

    Root<Block*> block;
    Block::buildModule(input, None, block);
    testEqual(repr(*block), expected);
}

testcase(block)
{
    testBuildModule("foo = 1\n"
                    "foo.bar",
                    "Const 1, SetGlobal foo, Pop, "
                    "GetGlobal foo, GetAttr bar, Return");

    testBuildModule("foo = 1\n"
                    "foo()",
                    "Const 1, SetGlobal foo, Pop, "
                    "GetGlobal foo, Call 0, Return");

    testBuildModule("foo = 1\n"
                    "bar = 1\n"
                    "baz = 1\n"
                    "foo(bar, baz)",
                    "Const 1, SetGlobal foo, Pop, "
                    "Const 1, SetGlobal bar, Pop, "
                    "Const 1, SetGlobal baz, Pop, "
                    "GetGlobal foo, GetGlobal bar, GetGlobal baz, Call 2, Return");

    testBuildModule("foo = 1\n"
                    "baz = 1\n"
                    "foo.bar(baz)",
                    "Const 1, SetGlobal foo, Pop, "
                    "Const 1, SetGlobal baz, Pop, "
                    "GetGlobal foo, GetMethod bar, GetGlobal baz, Call 2, Return");

    testBuildModule("foo = 1\n"
                    "baz = 1\n"
                    "foo.bar = baz",
                    "Const 1, SetGlobal foo, Pop, "
                    "Const 1, SetGlobal baz, Pop, "
                    "GetGlobal baz, GetGlobal foo, SetAttr bar, Return");

    testBuildModule("foo = 1\n"
                    "foo + 1",
                    "Const 1, SetGlobal foo, Pop, "
                    "GetGlobal foo, Const 1, BinaryOp +, Return");

    testBuildModule("1",
                    "Const 1, Return");

    testBuildModule("return 1",
                    "Const 1, Return");

    testBuildModule("foo = 1\n"
                    "bar = 1\n"
                    "return foo in bar",
                    "Const 1, SetGlobal foo, Pop, "
                    "Const 1, SetGlobal bar, Pop, "
                    "GetGlobal foo, GetGlobal bar, In, Return");

    testBuildModule("foo = 1\n"
                    "bar = 1\n"
                    "return foo is not bar",
                    "Const 1, SetGlobal foo, Pop, "
                    "Const 1, SetGlobal bar, Pop, "
                    "GetGlobal foo, GetGlobal bar, Is, Not, Return");

    testBuildModule("return 2 - - 1",
                    "Const 2, Const 1, GetMethod __neg__, Call 1, BinaryOp -, Return");
}
