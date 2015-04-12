#include "integer.h"

#include "bool.h"
#include "callable.h"
#include "singletons.h"
#include "string.h"
#include "value-inl.h"

#include <cmath>
#include <sstream>

struct UnaryPlus        { static int op(int a) { return a; } };
struct UnaryMinus       { static int op(int a) { return -a; } };
struct UnaryInvert      { static int op(int a) { return ~a; } };
struct BinaryPlus       { static int op(int a, int b) { return a + b; } };
struct BinaryMinus      { static int op(int a, int b) { return a - b; } };
struct BinaryMultiply   { static int op(int a, int b) { return a * b; } };
struct BinaryDivide     { static int op(int a, int b) { return a / b; } };
struct BinaryIntDivide  { static int op(int a, int b) { return a / b; } };
struct BinaryModulo     { static int op(int a, int b) { return a % b; } };
struct BinaryPower      { static int op(int a, int b) { return pow(a, b); } };
struct BinaryOr         { static int op(int a, int b) { return a | b; } };
struct BinaryXor        { static int op(int a, int b) { return a ^ b; } };
struct BinaryAnd        { static int op(int a, int b) { return a & b; } };
struct BinaryLeftShift  { static int op(int a, int b) { return a << b; } };
struct BinaryRightShift { static int op(int a, int b) { return a >> b; } };
struct CompareLT        { static int op(int a, int b) { return a < b; } };
struct CompareLE        { static int op(int a, int b) { return a <= b; } };
struct CompareGT        { static int op(int a, int b) { return a > b; } };
struct CompareGE        { static int op(int a, int b) { return a >= b; } };
struct CompareEQ        { static int op(int a, int b) { return a == b; } };
struct CompareNE        { static int op(int a, int b) { return a != b; } };

template <typename T>
static bool intUnaryOp(TracedVector<Value> args, Root<Value>& resultOut)
{
    if (args[0].isInt32())
        resultOut = Integer::get(T::op(args[0].asInt32()));
    else
        resultOut = NotImplemented;
    return true;
}

template <typename T>
static bool intBinaryOp(TracedVector<Value> args, Root<Value>& resultOut)
{
    if (args[0].isInt32() && args[1].isInt32())
        resultOut = Integer::get(T::op(args[0].asInt32(), args[1].asInt32()));
    else
        resultOut = NotImplemented;
    return true;
}

template <typename T>
static bool intCompareOp(TracedVector<Value> args, Root<Value>& resultOut)
{
    if (args[0].isInt32() && args[1].isInt32())
        resultOut = Boolean::get(T::op(args[0].asInt32(), args[1].asInt32()));
    else
        resultOut = NotImplemented;
    return true;
}

static bool intStr(TracedVector<Value> args, Root<Value>& resultOut) {
    if (!args[0].isInt32()) {
        resultOut = NotImplemented;
        return true;
    }

    ostringstream s;
    s << dec << args[0].asInt32();
    resultOut = String::get(s.str());
    return true;
}

GlobalRoot<Class*> Integer::ObjectClass;
GlobalRoot<Integer*> Integer::Zero;

void Integer::init()
{
    Root<Class*> cls(gc::create<Class>("int"));
    Root<Value> value;
    initNativeMethod(cls, "__str__", intStr, 1);
    initNativeMethod(cls, "__pos__", intUnaryOp<UnaryPlus>, 1);
    initNativeMethod(cls, "__neg__", intUnaryOp<UnaryMinus>, 1);
    initNativeMethod(cls, "__invert__", intUnaryOp<UnaryInvert>, 1);
    initNativeMethod(cls, "__lt__", intCompareOp<CompareLT>, 2);
    initNativeMethod(cls, "__le__", intCompareOp<CompareLE>, 2);
    initNativeMethod(cls, "__gt__", intCompareOp<CompareGT>, 2);
    initNativeMethod(cls, "__ge__", intCompareOp<CompareGE>, 2);
    initNativeMethod(cls, "__eq__", intCompareOp<CompareEQ>, 2);
    initNativeMethod(cls, "__ne__", intCompareOp<CompareNE>, 2);
    initNativeMethod(cls, "__or__", intBinaryOp<BinaryOr>, 2);
    initNativeMethod(cls, "__xor__", intBinaryOp<BinaryXor>, 2);
    initNativeMethod(cls, "__and__", intBinaryOp<BinaryAnd>, 2);
    initNativeMethod(cls, "__lshift__", intBinaryOp<BinaryLeftShift>, 2);
    initNativeMethod(cls, "__rshift__", intBinaryOp<BinaryRightShift>, 2);
    initNativeMethod(cls, "__add__", intBinaryOp<BinaryPlus>, 2);
    initNativeMethod(cls, "__sub__", intBinaryOp<BinaryMinus>, 2);
    initNativeMethod(cls, "__mul__", intBinaryOp<BinaryMultiply>, 2);
    initNativeMethod(cls, "__div__", intBinaryOp<BinaryDivide>, 2);
    initNativeMethod(cls, "__floordiv__", intBinaryOp<BinaryIntDivide>, 2);
    initNativeMethod(cls, "__mod__", intBinaryOp<BinaryModulo>, 2);
    initNativeMethod(cls, "__pow__", intBinaryOp<BinaryPower>, 2);
    ObjectClass.init(cls);

    Zero.init(gc::create<Integer>(0));
}

Integer::Integer(int v)
  : Object(ObjectClass), value_(v)
{}

Value Integer::get(int v)
{
    if (v == 0)
        return Value(Zero);
    else if (v >= INT16_MIN && v <= INT16_MAX)
        return Value(v);
    else
        return getObject(v);
}

Object* Integer::getObject(int v)
{
    return gc::create<Integer>(v);
}

void Integer::print(ostream& s) const {
    s << dec << value_;
}

#ifdef BUILD_TESTS

#include "test.h"

testcase(integer)
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
}

#endif
