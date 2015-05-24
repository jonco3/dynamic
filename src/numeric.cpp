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

typedef int32_t (IntUnaryOp)(int32_t);
static int32_t intPos(int32_t a) { return a; }
static int32_t intNeg(int32_t a) { return -a; }
static int32_t intInvert(int32_t a) { return ~a; }
static int32_t intHash(int32_t a) { return a; }

typedef int32_t (IntBinaryOp)(int32_t, int32_t);
static int32_t intAdd(int32_t a, int32_t b) { return a + b; }
static int32_t intSub(int32_t a, int32_t b) { return a - b; }
static int32_t intMul(int32_t a, int32_t b) { return a * b; }
static int32_t intDiv(int32_t a, int32_t b) { return a / b; } // todo
static int32_t intFloorDiv(int32_t a, int32_t b) { return a / b; }
static int32_t intMod(int32_t a, int32_t b) { return a % b; }
static int32_t intPow(int32_t a, int32_t b) { return (int)pow(a, b); }
static int32_t intOr(int32_t a, int32_t b) { return a | b; }
static int32_t intXor(int32_t a, int32_t b) { return a ^ b; }
static int32_t intAnd(int32_t a, int32_t b) { return a & b; }
static int32_t intLeftShift(int32_t a, int32_t b) { return a << b; }
static int32_t intRightShift(int32_t a, int32_t b) { return a >> b; }

typedef bool (IntCompareOp)(int32_t, int32_t);
static bool intLT(int32_t a, int32_t b) { return a < b; }
static bool intLE(int32_t a, int32_t b) { return a <= b; }
static bool intGT(int32_t a, int32_t b) { return a > b; }
static bool intGE(int32_t a, int32_t b) { return a >= b; }
static bool intEQ(int32_t a, int32_t b) { return a == b; }
static bool intNE(int32_t a, int32_t b) { return a != b; }

template <IntUnaryOp op>
static bool intUnaryOp(TracedVector<Value> args, Root<Value>& resultOut)
{
    if (!args[0].isInstanceOf(Integer::ObjectClass)) {
        resultOut = NotImplemented;
        return true;
    }

    resultOut = Integer::get(op(args[0].asInt32()));
    return true;
}

template <IntBinaryOp op>
static bool intBinaryOp(TracedVector<Value> args, Root<Value>& resultOut)
{
    if (!args[0].isInstanceOf(Integer::ObjectClass) ||
        !args[1].isInstanceOf(Integer::ObjectClass))
    {
        resultOut = NotImplemented;
        return true;
    }

    resultOut = Integer::get(op(args[0].asInt32(), args[1].asInt32()));
    return true;
}

template <IntCompareOp op>
static bool intCompareOp(TracedVector<Value> args, Root<Value>& resultOut)
{
    if (!args[0].isInstanceOf(Integer::ObjectClass) ||
        !args[1].isInstanceOf(Integer::ObjectClass))
    {
        resultOut = NotImplemented;
        return true;
    }

    resultOut = Boolean::get(op(args[0].asInt32(), args[1].asInt32()));
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
    initNativeMethod(cls, "__pos__", intUnaryOp<intPos>, 1);
    initNativeMethod(cls, "__neg__", intUnaryOp<intNeg>, 1);
    initNativeMethod(cls, "__invert__", intUnaryOp<intInvert>, 1);
    initNativeMethod(cls, "__lt__", intCompareOp<intLT>, 2);
    initNativeMethod(cls, "__le__", intCompareOp<intLE>, 2);
    initNativeMethod(cls, "__gt__", intCompareOp<intGT>, 2);
    initNativeMethod(cls, "__ge__", intCompareOp<intGE>, 2);
    initNativeMethod(cls, "__eq__", intCompareOp<intEQ>, 2);
    initNativeMethod(cls, "__ne__", intCompareOp<intNE>, 2);
    initNativeMethod(cls, "__or__", intBinaryOp<intOr>, 2);
    initNativeMethod(cls, "__xor__", intBinaryOp<intXor>, 2);
    initNativeMethod(cls, "__and__", intBinaryOp<intAnd>, 2);
    initNativeMethod(cls, "__lshift__", intBinaryOp<intLeftShift>, 2);
    initNativeMethod(cls, "__rshift__", intBinaryOp<intRightShift>, 2);
    initNativeMethod(cls, "__add__", intBinaryOp<intAdd>, 2);
    initNativeMethod(cls, "__sub__", intBinaryOp<intSub>, 2);
    initNativeMethod(cls, "__mul__", intBinaryOp<intMul>, 2);
    initNativeMethod(cls, "__div__", intBinaryOp<intDiv>, 2);
    initNativeMethod(cls, "__floordiv__", intBinaryOp<intFloorDiv>, 2);
    initNativeMethod(cls, "__mod__", intBinaryOp<intMod>, 2);
    initNativeMethod(cls, "__pow__", intBinaryOp<intPow>, 2);
    initNativeMethod(cls, "__hash__", intUnaryOp<intHash>, 1);
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

static double floatHash(double a)
{
    union PunnedDouble {
        double d;
        uint64_t i;
    };

    PunnedDouble p = { a };
    return p.i;
}

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

static bool floatValue(Traced<Value> value, double& out)
{
    if (value.isInstanceOf(Float::ObjectClass))
        out = value.asObject()->as<Float>()->value;
    else
        return false;

    return true;
}

static bool floatOrIntValue(Traced<Value> value, double& out)
{
    if (value.isInstanceOf(Float::ObjectClass))
        out = value.asObject()->as<Float>()->value;
    else if (value.isInt32())
        out = value.asInt32();
    else
        return false;

    return true;
}

template <FloatUnaryOp op>
static bool floatUnaryOp(TracedVector<Value> args, Root<Value>& resultOut)
{
    double a;
    if (!floatValue(args[0], a)) {
        resultOut = NotImplemented;
        return true;
    }

    resultOut = Float::get(op(floatValue(args[0])));
    return true;
}

template <FloatBinaryOp op>
static bool floatBinaryOp(TracedVector<Value> args, Root<Value>& resultOut)
{
    double a;
    if (!floatValue(args[0], a)) {
        resultOut = NotImplemented;
        return true;
    }

    double b;
    if (!floatOrIntValue(args[1], b)) {
        resultOut = NotImplemented;
        return true;
    }

    resultOut = Float::get(op(a, b));
    return true;
}

template <FloatBinaryOp op>
static bool floatRBinaryOp(TracedVector<Value> args, Root<Value>& resultOut)
{
    double a;
    if (!floatValue(args[0], a)) {
        resultOut = NotImplemented;
        return true;
    }

    double b;
    if (!floatOrIntValue(args[1], b)) {
        resultOut = NotImplemented;
        return true;
    }

    resultOut = Float::get(op(b, a));
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
    initNativeMethod(cls, "__radd__", floatRBinaryOp<floatAdd>, 2);
    initNativeMethod(cls, "__rsub__", floatRBinaryOp<floatSub>, 2);
    initNativeMethod(cls, "__rmul__", floatRBinaryOp<floatMul>, 2);
    initNativeMethod(cls, "__rdiv__", floatRBinaryOp<floatDiv>, 2);
    initNativeMethod(cls, "__rfloordiv__", floatRBinaryOp<floatFloorDiv>, 2);
    initNativeMethod(cls, "__rmod__", floatRBinaryOp<floatMod>, 2);
    initNativeMethod(cls, "__rpow__", floatRBinaryOp<floatPow>, 2);
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
    testInterp("2 + 0.5", "2.5");
    testInterp("0.5 + 2", "2.5");
}

#endif
