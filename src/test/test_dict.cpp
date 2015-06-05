#include "src/dict.h"

#include "src/interp.h"
#include "src/test.h"

testcase(dict)
{
    testInterp("{}", "{}");
    testInterp("{1: 2}", "{1: 2}");
    testInterp("{1: 2,}", "{1: 2}");
    testInterp("{'foo': 2 + 2}", "{'foo': 4}");
    testInterp("{2 + 2: 'bar'}", "{4: 'bar'}");
    testInterp("1 in {1: 2}", "True");
    testInterp("2 in {1: 2}", "False");

    testException("{}[0]", "KeyError: 0");
    testInterp("{'baz': 2}['baz']", "2");

    testInterp("{}[1] = 2", "2");
    testInterp("a = {}; a[1] = 2; a", "{1: 2}");
    testInterp("a = {1: 2}; a[1] = 3; a", "{1: 3}");
    testInterp("{1: 2}.keys()", "(1,)");
    testInterp("{1: 2}.values()", "(2,)");
}
