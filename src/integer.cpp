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
    initNativeMethod(cls, "__str__", 1, intStr);
    initNativeMethod(cls, "__pos__", 1, intUnaryOp<UnaryPlus>);
    initNativeMethod(cls, "__neg__", 1, intUnaryOp<UnaryMinus>);
    initNativeMethod(cls, "__invert__", 1, intUnaryOp<UnaryInvert>);
    initNativeMethod(cls, "__lt__", 2, intCompareOp<CompareLT>);
    initNativeMethod(cls, "__le__", 2, intCompareOp<CompareLE>);
    initNativeMethod(cls, "__gt__", 2, intCompareOp<CompareGT>);
    initNativeMethod(cls, "__ge__", 2, intCompareOp<CompareGE>);
    initNativeMethod(cls, "__eq__", 2, intCompareOp<CompareEQ>);
    initNativeMethod(cls, "__ne__", 2, intCompareOp<CompareNE>);
    initNativeMethod(cls, "__or__", 2, intBinaryOp<BinaryOr>);
    initNativeMethod(cls, "__xor__", 2, intBinaryOp<BinaryXor>);
    initNativeMethod(cls, "__and__", 2, intBinaryOp<BinaryAnd>);
    initNativeMethod(cls, "__lshift__", 2, intBinaryOp<BinaryLeftShift>);
    initNativeMethod(cls, "__rshift__", 2, intBinaryOp<BinaryRightShift>);
    initNativeMethod(cls, "__add__", 2, intBinaryOp<BinaryPlus>);
    initNativeMethod(cls, "__sub__", 2, intBinaryOp<BinaryMinus>);
    initNativeMethod(cls, "__mul__", 2, intBinaryOp<BinaryMultiply>);
    initNativeMethod(cls, "__div__", 2, intBinaryOp<BinaryDivide>);
    initNativeMethod(cls, "__floordiv__", 2, intBinaryOp<BinaryIntDivide>);
    initNativeMethod(cls, "__mod__", 2, intBinaryOp<BinaryModulo>);
    initNativeMethod(cls, "__pow__", 2, intBinaryOp<BinaryPower>);
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
