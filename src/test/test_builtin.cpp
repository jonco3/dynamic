#include "src/builtin.h"

#include "src/test.h"

#include "test_interp.h"

testcase(builtin)
{
    testInterp("hasattr(1, 'foo')", "False");
    testInterp("hasattr(1, '__repr__')", "True");
    testInterp("range(4, 7)", "[4, 5, 6]");
}
