#include "numeric.h"

#include "bool.h"
#include "callable.h"
#include "exception.h"
#include "singletons.h"
#include "string.h"
#include "value-inl.h"

#include <cmath>
#include <sstream>

GlobalRoot<Class*> Integer::ObjectClass;
GlobalRoot<Class*> Float::ObjectClass;

typedef int64_t (IntUnaryOp)(int64_t);
static int64_t intPos(int64_t a) { return a; }
static int64_t intNeg(int64_t a) { return -a; }
static int64_t intInvert(int64_t a) { return ~a; }
static int64_t intHash(int64_t a) { return a; }

typedef int64_t (IntBinaryOp)(int64_t, int64_t);
static int64_t intAdd(int64_t a, int64_t b) { return a + b; }
static int64_t intSub(int64_t a, int64_t b) { return a - b; }
static int64_t intMul(int64_t a, int64_t b) { return a * b; }
static int64_t intFloorDiv(int64_t a, int64_t b) { return a / b; }
static int64_t intMod(int64_t a, int64_t b) { return a % b; }
static int64_t intPow(int64_t a, int64_t b) { return (int)pow(a, b); }
static int64_t intOr(int64_t a, int64_t b) { return a | b; }
static int64_t intXor(int64_t a, int64_t b) { return a ^ b; }
static int64_t intAnd(int64_t a, int64_t b) { return a & b; }
static int64_t intLeftShift(int64_t a, int64_t b) { return a << b; }
static int64_t intRightShift(int64_t a, int64_t b) { return a >> b; }

typedef bool (IntCompareOp)(int64_t, int64_t);
static bool intLT(int64_t a, int64_t b) { return a < b; }
static bool intLE(int64_t a, int64_t b) { return a <= b; }
static bool intGT(int64_t a, int64_t b) { return a > b; }
static bool intGE(int64_t a, int64_t b) { return a >= b; }
static bool intEQ(int64_t a, int64_t b) { return a == b; }
static bool intNE(int64_t a, int64_t b) { return a != b; }

template <IntUnaryOp op>
static bool intUnaryOp(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    if (!args[0].isInstanceOf(Integer::ObjectClass)) {
        resultOut = NotImplemented;
        return true;
    }

    resultOut = Integer::get(op(args[0].toInt()));
    return true;
}

template <IntBinaryOp op>
static bool intBinaryOp(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    if (!args[0].isInstanceOf(Integer::ObjectClass) ||
        !args[1].isInstanceOf(Integer::ObjectClass))
    {
        resultOut = NotImplemented;
        return true;
    }

    resultOut = Integer::get(op(args[0].toInt(), args[1].toInt()));
    return true;
}

template <IntCompareOp op>
static bool intCompareOp(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    if (!args[0].isInstanceOf(Integer::ObjectClass) ||
        !args[1].isInstanceOf(Integer::ObjectClass))
    {
        resultOut = NotImplemented;
        return true;
    }

    resultOut = Boolean::get(op(args[0].toInt(), args[1].toInt()));
    return true;
}

static bool intTrueDiv(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    if (!args[0].isInstanceOf(Integer::ObjectClass) ||
        !args[1].isInstanceOf(Integer::ObjectClass))
    {
        resultOut = NotImplemented;
        return true;
    }

    resultOut = Float::get(double(args[0].toInt()) / args[1].toInt());
    return true;
}

static bool intNew(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], Class::ObjectClass, resultOut))
        return false;

    if (args.size() == 1) {
        resultOut = gc.create<Integer>();
        return true;
    }

    Stack<Value> arg(args[1]);
    if (arg.isInstanceOf(Integer::ObjectClass)) {
        resultOut = arg;
    } else if (arg.isInstanceOf(String::ObjectClass)) {
        string str = arg.asObject()->as<String>()->value();
        int value = 0;
        size_t pos = 0;
        bool ok = true;
        try {
            value = stoi(str, &pos);
        } catch (const logic_error& e) {
            ok = false;
        }
        if (!ok || pos < str.size()) {
            string message = "could not convert string to int: '" + str + "'";
            resultOut = gc.create<ValueError>(message);
            return false;
        }
        resultOut = Integer::get(value);
    } else {
        string message =
            "int() argument must be a string or a number, not '" +
            arg.type()->name() + "'";
        resultOut = gc.create<TypeError>(message);
        return false;
    }

    return true;
}

void Integer::init()
{
    Stack<Class*> cls(Class::createNative("int", intNew, 2));
    Stack<Value> value;
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
    initNativeMethod(cls, "__truediv__", intTrueDiv, 2);
    initNativeMethod(cls, "__floordiv__", intBinaryOp<intFloorDiv>, 2);
    initNativeMethod(cls, "__mod__", intBinaryOp<intMod>, 2);
    initNativeMethod(cls, "__pow__", intBinaryOp<intPow>, 2);
    initNativeMethod(cls, "__hash__", intUnaryOp<intHash>, 1);
    ObjectClass.init(cls);
}

Integer::Integer(int64_t v)
  : Object(ObjectClass), value_(v)
{}

Integer::Integer(Traced<Class*> cls)
  : Object(cls)
{}

Value Integer::get(int64_t v)
{
    if (v >= INT32_MIN && v <= INT32_MAX)
        return Value(static_cast<int32_t>(v));
    else
        return getObject(v);
}

Object* Integer::getObject(int64_t v)
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
static double floatTrueDiv(double a, double b) { return a / b; }
static double floatFloorDiv(double a, double b) { return floor(a / b); }
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
    return value.toFloat();
}

static bool floatValue(Traced<Value> value, double& out)
{
    if (value.isFloat())
        out = value.toFloat();
    else
        return false;

    return true;
}

static bool floatOrIntValue(Traced<Value> value, double& out)
{
    if (value.isFloat())
        out = value.toFloat();
    else if (value.isInt())
        out = value.toInt();
    else
        return false;

    return true;
}

template <FloatUnaryOp op>
static bool floatUnaryOp(TracedVector<Value> args, MutableTraced<Value> resultOut)
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
static bool floatBinaryOp(TracedVector<Value> args, MutableTraced<Value> resultOut)
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
static bool floatRBinaryOp(TracedVector<Value> args, MutableTraced<Value> resultOut)
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
static bool floatCompareOp(TracedVector<Value> args, MutableTraced<Value> resultOut)
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

static bool floatNew(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], Class::ObjectClass, resultOut))
        return false;

    if (args.size() == 1) {
        resultOut = Float::get(0.0);
        return true;
    }

    Stack<Value> arg(args[1]);
    if (arg.isInt()) {
        resultOut = Float::get(arg.toInt());
    } else if (arg.isFloat()) {
        resultOut = Float::get(arg.toFloat());
    } else if (arg.isInstanceOf(String::ObjectClass)) {
        string str = arg.asObject()->as<String>()->value();
        double value = 0;
        size_t pos = 0;
        bool ok = true;
        try {
            value = stod(str, &pos);
        } catch (const logic_error& e) {
            ok = false;
        }
        if (!ok || pos < str.size()) {
            string message = "could not convert string to float: '" + str + "'";
            resultOut = gc.create<ValueError>(message);
            return false;
        }
        resultOut = Float::get(value);
    } else {
        string message =
            "float() argument must be a string or a number, not '" +
            arg.type()->name() + "'";
        resultOut = gc.create<TypeError>(message);
        return false;
    }

    return true;
}

void Float::init()
{
    Stack<Class*> cls(Class::createNative("float", floatNew, 2));
    Stack<Value> value;
    initNativeMethod(cls, "__pos__", floatUnaryOp<floatPos>, 1);
    initNativeMethod(cls, "__neg__", floatUnaryOp<floatNeg>, 1);
    initNativeMethod(cls, "__hash__", floatUnaryOp<floatHash>, 1);
    initNativeMethod(cls, "__add__", floatBinaryOp<floatAdd>, 2);
    initNativeMethod(cls, "__sub__", floatBinaryOp<floatSub>, 2);
    initNativeMethod(cls, "__mul__", floatBinaryOp<floatMul>, 2);
    initNativeMethod(cls, "__truediv__", floatBinaryOp<floatTrueDiv>, 2);
    initNativeMethod(cls, "__floordiv__", floatBinaryOp<floatFloorDiv>, 2);
    initNativeMethod(cls, "__mod__", floatBinaryOp<floatMod>, 2);
    initNativeMethod(cls, "__pow__", floatBinaryOp<floatPow>, 2);
    initNativeMethod(cls, "__radd__", floatRBinaryOp<floatAdd>, 2);
    initNativeMethod(cls, "__rsub__", floatRBinaryOp<floatSub>, 2);
    initNativeMethod(cls, "__rmul__", floatRBinaryOp<floatMul>, 2);
    initNativeMethod(cls, "__rtruediv__", floatRBinaryOp<floatTrueDiv>, 2);
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
  : Object(ObjectClass), value_(v)
{}

Float::Float(Traced<Class*> cls)
  : Object(cls), value_(0)
{}

void Float::print(ostream& s) const {
    char buffer[32];
    sprintf(buffer, "%.17g", value_);
    s << buffer;
}

Value Float::get(double v)
{
    return Value(v);
}

Object* Float::getObject(double v)
{
    return gc.create<Float>(v);
}
