#include "src/numeric.h"

#include "src/test.h"

#include "test_interp.h"

testcase(numeric)
{
    testFalse(Integer::get(1).isObject());
    testFalse(Integer::get(32767).isObject());
    testTrue(Integer::get(32768).isObject());

    testInterp("1", "1");
    testInterp("2 + 2", "4");
    testInterp("5 - 2", "3");
    testInterp("1 + 0", "1");
    testInterp("+3", "3");
    testInterp("-4", "-4");
    testInterp("32767 + 1", "32768");
    testInterp("32768 - 1", "32767");

    testInterp("2.5", "2.5");
    testInterp("2.0 + 0.5", "2.5");
    testInterp("2 + 0.5", "2.5");
    testInterp("0.5 + 2", "2.5");
}
