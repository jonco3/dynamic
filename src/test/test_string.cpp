#include "../string.h"

#include "../test.h"

#include "test_interp.h"

testcase(string)
{
    testInterp("\"foo\"", "'foo'");
    testInterp("'foo'", "'foo'");
    testInterp("\"\"", "''");
    testInterp("''", "''");
    testInterp("\"foo\" + 'bar'", "'foobar'");
}
