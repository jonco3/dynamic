#include "numeric.h"

#include "bool.h"
#include "callable.h"
#include "singletons.h"
#include "string.h"
#include "value-inl.h"

#include <cmath>
#include <sstream>

GlobalRoot<Class*> Integer::ObjectClass;
GlobalRoot<Integer*> Integer::Zero;
GlobalRoot<Class*> Float::ObjectClass;

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
struct Hash             { static int op(int a) { return a; } };

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

void Integer::init()
{
    Root<Class*> cls(gc.create<Class>("int"));
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
    initNativeMethod(cls, "__hash__", intUnaryOp<Hash>, 1);
    ObjectClass.init(cls);

    Zero.init(gc.create<Integer>(0));
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
    return gc.create<Integer>(v);
}

void Integer::print(ostream& s) const {
    s << dec << value_;
}

typedef double (FloatUnaryOp)(double);
static double floatPos(double a) { return a; }
static double floatNeg(double a) { return -a; }
static double floatHash(double a) { return *(uint64_t*)&a; }

typedef double (FloatBinaryOp)(double, double);
static double floatAdd(double a, double b) { return a + b; }
static double floatSub(double a, double b) { return a - b; }
static double floatMul(double a, double b) { return a * b; }
static double floatDiv(double a, double b) { return a / b; }
static double floatFloorDiv(double a, double b) { return a / b; } // todo
static double floatMod(double a, double b) { return fmod(a, b); }
static double floatPow(double a, double b) { return pow(a, b); }

typedef bool (FloatCompareOp)(double, double);
static bool floatLT(double a, double b) { return a < b; }
static bool floatLE(double a, double b) { return a <= b; }
static bool floatGT(double a, double b) { return a > b; }
static bool floatGE(double a, double b) { return a >= b; }
static bool floatEQ(double a, double b) { return a == b; }
static bool floatNE(double a, double b) { return a != b; }

static double floatValue(Traced<Value> value)
{
    return value.asObject()->as<Float>()->value;
}

template <FloatUnaryOp op>
static bool floatUnaryOp(TracedVector<Value> args, Root<Value>& resultOut)
{
    if (!args[0].isInstanceOf(Float::ObjectClass)) {
        resultOut = NotImplemented;
        return true;
    }

    resultOut = Float::get(op(floatValue(args[0])));
    return true;
}

template <FloatBinaryOp op>
static bool floatBinaryOp(TracedVector<Value> args, Root<Value>& resultOut)
{
    if (!args[0].isInstanceOf(Float::ObjectClass) ||
        !args[1].isInstanceOf(Float::ObjectClass))
    {
        resultOut = NotImplemented;
        return true;
    }

    resultOut = Float::get(op(floatValue(args[0]), floatValue(args[1])));
    return true;
}

template <FloatCompareOp op>
static bool floatCompareOp(TracedVector<Value> args, Root<Value>& resultOut)
{
    if (!args[0].isInstanceOf(Float::ObjectClass) ||
        !args[1].isInstanceOf(Float::ObjectClass))
    {
        resultOut = NotImplemented;
        return true;
    }

    resultOut = Boolean::get(op(floatValue(args[0]), floatValue(args[1])));
    return true;
}

static bool floatStr(TracedVector<Value> args, Root<Value>& resultOut) {
    if (!checkInstanceOf(args[0], Float::ObjectClass, resultOut))
        return false;

    ostringstream s;
    s << floatValue(args[0]);
    resultOut = String::get(s.str());
    return true;
}

void Float::init()
{
    Root<Class*> cls(gc.create<Class>("int"));
    Root<Value> value;
    initNativeMethod(cls, "__str__", floatStr, 1);
    initNativeMethod(cls, "__pos__", floatUnaryOp<floatPos>, 1);
    initNativeMethod(cls, "__neg__", floatUnaryOp<floatNeg>, 1);
    initNativeMethod(cls, "__hash__", floatUnaryOp<floatHash>, 1);
    initNativeMethod(cls, "__add__", floatBinaryOp<floatAdd>, 2);
    initNativeMethod(cls, "__sub__", floatBinaryOp<floatSub>, 2);
    initNativeMethod(cls, "__mul__", floatBinaryOp<floatMul>, 2);
    initNativeMethod(cls, "__div__", floatBinaryOp<floatDiv>, 2);
    initNativeMethod(cls, "__floordiv__", floatBinaryOp<floatFloorDiv>, 2);
    initNativeMethod(cls, "__mod__", floatBinaryOp<floatMod>, 2);
    initNativeMethod(cls, "__pow__", floatBinaryOp<floatPow>, 2);
    initNativeMethod(cls, "__lt__", floatCompareOp<floatLT>, 2);
    initNativeMethod(cls, "__le__", floatCompareOp<floatLE>, 2);
    initNativeMethod(cls, "__gt__", floatCompareOp<floatGT>, 2);
    initNativeMethod(cls, "__ge__", floatCompareOp<floatGE>, 2);
    initNativeMethod(cls, "__eq__", floatCompareOp<floatEQ>, 2);
    initNativeMethod(cls, "__ne__", floatCompareOp<floatNE>, 2);
    ObjectClass.init(cls);
}

Float::Float(double v)
  : Object(ObjectClass), value(v)
{}

void Float::print(ostream& s) const {
    s << value;
}

Float* Float::get(double v)
{
    return gc.create<Float>(v);
}

#ifdef BUILD_TESTS

#include "test.h"

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
}

#endif
