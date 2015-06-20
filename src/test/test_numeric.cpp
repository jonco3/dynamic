#include "src/numeric.h"
#include "src/value-inl.h"

#include "src/test.h"

#include "test_interp.h"

static void testIntIsInt32(int64_t i)
{
    Value v = Integer::get(i);
    testTrue(v.isInt());
    testTrue(v.isInt32());
    testFalse(v.isObject());
    testEqual(v.asInt32(), i);
    testEqual(v.toInt(), i);
}

static void testIntIsObject(int64_t i)
{
    Value v = Integer::get(i);
    testTrue(v.isInt());
    testFalse(v.isInt32());
    testTrue(v.isObject());
    testEqual(v.toInt(), i);
}

testcase(numeric)
{
    testIntIsInt32(0);
    testIntIsInt32(INT32_MIN);
    testIntIsInt32(INT32_MAX);
    testIntIsObject((int64_t)INT32_MIN - 1);
    testIntIsObject((int64_t)INT32_MAX + 1);

    testInterp("1", "1");
    testInterp("2 + 2", "4");
    testInterp("5 - 2", "3");
    testInterp("1 + 0", "1");
    testInterp("+3", "3");
    testInterp("-4", "-4");

    testEqual(INT32_MAX, 2147483647ll);
    testEqual(INT32_MIN, -2147483648ll);
    testInterp("2147483647 + 1", "2147483648");
    testInterp("2147483648 - 1", "2147483647");
    testInterp("-2147483648 - 1", "-2147483649");
    testInterp("-2147483649 + 1", "-2147483648");

    testInterp("2.5", "2.5");
    testInterp("2.0 + 0.5", "2.5");
    testInterp("2 + 0.5", "2.5");
    testInterp("0.5 + 2", "2.5");

    testInterp("int()", "0");
    testInterp("int(1)", "1");
    testException("int('')", "could not convert string to int");
    testException("int('1foo')", "could not convert string to int");

    testInterp("float()", "0");
    testInterp("float(1)", "1");
    testInterp("float(1.5)", "1.5");
    testInterp("float('1.5')", "1.5");
    testException("float('')", "could not convert string to float");
    testException("float('3.1foo')", "could not convert string to float");
}
