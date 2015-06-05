#include "src/builtin.h"

#include "src/test.h"

#include "test_interp.h"

testcase(builtin)
{
    testInterp("hasattr(1, 'foo')", "False");
    testInterp("hasattr(1, '__str__')", "True");
    testInterp("a = object()\n"
               "a.foo = 1\n"
               "a.foo",
               "1");
    testInterp("range(4, 7)", "[4, 5, 6]");
}
