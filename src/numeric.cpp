#include "numeric.h"

#include "callable.h"
#include "exception.h"
#include "singletons.h"
#include "string.h"
#include "value-inl.h"

#include <cmath>
#include <sstream>

GlobalRoot<Class*> Integer::ObjectClass;
GlobalRoot<Class*> Float::ObjectClass;
GlobalRoot<Class*> Boolean::ObjectClass;

GlobalRoot<Boolean*> Boolean::True;
GlobalRoot<Boolean*> Boolean::False;

typedef int32_t (Int32UnaryOp)(int32_t);
typedef mpz_class (MPZUnaryOp)(const mpz_class&);

static inline int32_t intPos(int32_t a) { return a; }
static inline int32_t intNeg(int32_t a) { return -a; }
static inline int32_t intInvert(int32_t a) { return ~a; }
static inline int32_t intHash(int32_t a) { return a; }

static inline mpz_class mpzPos(const mpz_class& a) { return a; }

static inline mpz_class mpzNeg(const mpz_class& a)
{
    mpz_class r;
    mpz_neg(r.get_mpz_t(), a.get_mpz_t());
    return r;
}

static inline mpz_class mpzInvert(const mpz_class& a)
{
    mpz_class r;
    mpz_com(r.get_mpz_t(), a.get_mpz_t());
    return r;
}

static inline mpz_class mpzHash(const mpz_class& a) { return a; }

template <Int32UnaryOp int32op, MPZUnaryOp mpzOp>
static bool intUnaryOp(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    if (!args[0].isInstanceOf(Integer::ObjectClass)) {
        resultOut = NotImplemented;
        return true;
    }

    if (args[0].isInt32())
        resultOut = Integer::get(int32op(args[0].asInt32()));
    else
        resultOut = Integer::get(mpzOp(args[0].as<Integer>()->value()));

    return true;
}

template <BinaryOp Op>
static Value mpzBinaryOp(const mpz_class& a, const mpz_class& b);

template <> inline Value mpzBinaryOp<BinaryAdd>(const mpz_class& a, const mpz_class& b)
{
    return Integer::get(a + b);
}

template <> inline Value mpzBinaryOp<BinarySub>(const mpz_class& a, const mpz_class& b)
{
    return Integer::get(a - b);
}

template <> inline Value mpzBinaryOp<BinaryMul>(const mpz_class& a, const mpz_class& b)
{
    return Integer::get(a * b);
}

template <> inline Value mpzBinaryOp<BinaryTrueDiv>(const mpz_class& a, const mpz_class& b)
{
    return Float::get(a.get_d() / b.get_d());
}

template <> inline Value mpzBinaryOp<BinaryFloorDiv>(const mpz_class& a, const mpz_class& b)
{
    return Integer::get(a / b);
}

template <> inline Value mpzBinaryOp<BinaryModulo>(const mpz_class& a, const mpz_class& b)
{
    return Integer::get(a % b);
}

template <> inline Value mpzBinaryOp<BinaryPower>(const mpz_class& a, const mpz_class& b)
{
    assert(b.fits_sint_p()); // todo: raise error
    mpz_class r;
    mpz_pow_ui(r.get_mpz_t(), a.get_mpz_t(), b.get_si());
    return Integer::get(r);
}

template <> inline Value mpzBinaryOp<BinaryOr>(const mpz_class& a, const mpz_class& b)
{
    return Integer::get(a | b);
}

template <> inline Value mpzBinaryOp<BinaryXor>(const mpz_class& a, const mpz_class& b)
{
    return Integer::get(a ^ b);
}

template <> inline Value mpzBinaryOp<BinaryAnd>(const mpz_class& a, const mpz_class& b)
{
    return Integer::get(a & b);
}

template <> Value mpzBinaryOp<BinaryLeftShift>(const mpz_class& a, const mpz_class& b)
{
    assert(b.fits_sint_p()); // todo
    mpz_class r;
    mpz_mul_2exp(r.get_mpz_t(), a.get_mpz_t(), b.get_si());
    return Integer::get(r);
}

template <> Value mpzBinaryOp<BinaryRightShift>(const mpz_class& a, const mpz_class& b)
{
    assert(b.fits_sint_p()); // todo
    mpz_class r;
    mpz_div_2exp(r.get_mpz_t(), a.get_mpz_t(), b.get_si());
    // todo: rounds towards zero
    return Integer::get(r);
}

template <CompareOp Op>
static Value mpzCompareOp(const mpz_class& a, const mpz_class& b);

template <> inline Value mpzCompareOp<CompareLT>(const mpz_class& a, const mpz_class& b)
{
    return Boolean::get(a < b);
}

template <> inline Value mpzCompareOp<CompareLE>(const mpz_class& a, const mpz_class& b)
{
    return Boolean::get(a <= b);
}

template <> inline Value mpzCompareOp<CompareGT>(const mpz_class& a, const mpz_class& b)
{
    return Boolean::get(a > b);
}

template <> inline Value mpzCompareOp<CompareGE>(const mpz_class& a, const mpz_class& b)
{
    return Boolean::get(a >= b);
}

template <> inline Value mpzCompareOp<CompareEQ>(const mpz_class& a, const mpz_class& b)
{
    return Boolean::get(a == b);
}

template <> inline Value mpzCompareOp<CompareNE>(const mpz_class& a, const mpz_class& b)
{
    return Boolean::get(a != b);
}

template <BinaryOp Op>
static bool intBinaryOp(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    if (!args[0].isInstanceOf(Integer::ObjectClass) ||
        !args[1].isInstanceOf(Integer::ObjectClass))
    {
        resultOut = NotImplemented;
        return true;
    }

    if (args[0].isInt32() && args[1].isInt32()) {
        resultOut = Integer::binaryOp<Op>(args[0].asInt32(), args[1].asInt32());
        return true;
    }

    if (!args[0].isInt32() && !args[1].isInt32()) {
        resultOut = mpzBinaryOp<Op>(args[0].as<Integer>()->value(),
                                    args[1].as<Integer>()->value());
        return true;
    }

    resultOut = mpzBinaryOp<Op>(args[0].toInt(), args[1].toInt());
    return true;
}

template <CompareOp Op>
static bool intCompareOp(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    if (!args[0].isInstanceOf(Integer::ObjectClass) ||
        !args[1].isInstanceOf(Integer::ObjectClass))
    {
        resultOut = NotImplemented;
        return true;
    }

    if (args[0].isInt32() && args[1].isInt32()) {
        resultOut = Integer::compareOp<Op>(args[0].asInt32(),
                                           args[1].asInt32());
        return true;
    }

    if (!args[0].isInt32() && !args[1].isInt32()) {
        resultOut = mpzCompareOp<Op>(args[0].as<Integer>()->value(),
                                     args[1].as<Integer>()->value());
        return true;
    }

    resultOut = mpzCompareOp<Op>(args[0].toInt(), args[1].toInt());
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
    initNativeMethod(cls, "__pos__", intUnaryOp<intPos, mpzPos>, 1);
    initNativeMethod(cls, "__neg__", intUnaryOp<intNeg, mpzNeg>, 1);
    initNativeMethod(cls, "__invert__", intUnaryOp<intInvert, mpzInvert>, 1);
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
    initNativeMethod(cls, "__add__", intBinaryOp<BinaryAdd>, 2);
    initNativeMethod(cls, "__sub__", intBinaryOp<BinarySub>, 2);
    initNativeMethod(cls, "__mul__", intBinaryOp<BinaryMul>, 2);
    initNativeMethod(cls, "__truediv__", intBinaryOp<BinaryTrueDiv>, 2);
    initNativeMethod(cls, "__floordiv__", intBinaryOp<BinaryFloorDiv>, 2);
    initNativeMethod(cls, "__mod__", intBinaryOp<BinaryModulo>, 2);
    initNativeMethod(cls, "__pow__", intBinaryOp<BinaryPower>, 2);
    initNativeMethod(cls, "__hash__", intUnaryOp<intHash, mpzHash>, 1);
    ObjectClass.init(cls);
}

static void mpz_set_si64(mpz_t n, int64_t i)
{
    mpz_set_si(n, (int32_t)(i >> 32));
    mpz_mul_2exp(n, n, 32);
    mpz_add_ui(n, n, (uint32_t)i);
}

static mpz_class mpzFromInt64(int64_t i)
{
    mpz_class n;
    mpz_set_si64(n.get_mpz_t(), i);
    return n;
}

Integer::Integer(int64_t v)
  : Object(ObjectClass), value_(mpzFromInt64(v))
{}

Integer::Integer(const mpz_class& v)
  : Object(ObjectClass), value_(v)
{}

Integer::Integer(Traced<Class*> cls, int64_t v)
  : Object(cls), value_(mpzFromInt64(v))
{}

Object* Integer::getObject(int64_t v)
{
    return gc.create<Integer>(v);
}

Object* Integer::getObject(const mpz_class& v)
{
    return gc.create<Integer>(v);
}

void Integer::print(ostream& s) const {
    s << dec << value_;
}

static bool boolNew(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], Class::ObjectClass, resultOut))
        return false;

    if (args.size() == 1) {
        resultOut = Boolean::False;
        return true;
    }

    // todo: convert argument to bool

    return true;
}

bool Boolean::ClassInitialised = false;

void Boolean::init()
{
    assert(!ClassInitialised);
    ObjectClass.init(Class::createNative("bool", boolNew, 1,
                                         Integer::ObjectClass));
    True.init(gc.create<Boolean>(1));
    False.init(gc.create<Boolean>(0));
    ClassInitialised = true;
}

Boolean::Boolean(int64_t value)
  : Integer(ObjectClass, value)
{
    assert(!ClassInitialised);
    assert(value == 0 || value == 1);
}

void Boolean::print(ostream& s) const
{
    s << (value() == 0 ? "False" : "True");
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
    else if (value.isInt32())
        out = value.asInt32();
    else if (value.isInstanceOf<Integer>())
        out = value.as<Integer>()->value().get_d();
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

    resultOut = Float::get(op(a));
    return true;
}

template <BinaryOp Op>
static bool
floatBinaryOp(TracedVector<Value> args, MutableTraced<Value> resultOut)
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

    resultOut = Float::binaryOp<Op>(a, b);
    return true;
}

template <BinaryOp Op>
static bool
floatRBinaryOp(TracedVector<Value> args, MutableTraced<Value> resultOut)
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

    resultOut = Float::binaryOp<Op>(b, a);
    return true;
}

template <CompareOp Op>
static bool floatCompareOp(TracedVector<Value> args, MutableTraced<Value> resultOut)
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

    resultOut = Float::compareOp<Op>(a, b);
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
    if (arg.isInt32()) {
        resultOut = Float::get(arg.asInt32());
    } else if (arg.is<Integer>()) {
        resultOut = Float::get(arg.as<Integer>()->value().get_d());
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
    initNativeMethod(cls, "__add__", floatBinaryOp<BinaryAdd>, 2);
    initNativeMethod(cls, "__sub__", floatBinaryOp<BinarySub>, 2);
    initNativeMethod(cls, "__mul__", floatBinaryOp<BinaryMul>, 2);
    initNativeMethod(cls, "__truediv__", floatBinaryOp<BinaryTrueDiv>, 2);
    initNativeMethod(cls, "__floordiv__", floatBinaryOp<BinaryFloorDiv>, 2);
    initNativeMethod(cls, "__mod__", floatBinaryOp<BinaryModulo>, 2);
    initNativeMethod(cls, "__pow__", floatBinaryOp<BinaryPower>, 2);
    initNativeMethod(cls, "__radd__", floatRBinaryOp<BinaryAdd>, 2);
    initNativeMethod(cls, "__rsub__", floatRBinaryOp<BinarySub>, 2);
    initNativeMethod(cls, "__rmul__", floatRBinaryOp<BinaryMul>, 2);
    initNativeMethod(cls, "__rtruediv__", floatRBinaryOp<BinaryTrueDiv>, 2);
    initNativeMethod(cls, "__rfloordiv__", floatRBinaryOp<BinaryFloorDiv>, 2);
    initNativeMethod(cls, "__rmod__", floatRBinaryOp<BinaryModulo>, 2);
    initNativeMethod(cls, "__rpow__", floatRBinaryOp<BinaryPower>, 2);
    initNativeMethod(cls, "__lt__", floatCompareOp<CompareLT>, 2);
    initNativeMethod(cls, "__le__", floatCompareOp<CompareLE>, 2);
    initNativeMethod(cls, "__gt__", floatCompareOp<CompareGT>, 2);
    initNativeMethod(cls, "__ge__", floatCompareOp<CompareGE>, 2);
    initNativeMethod(cls, "__eq__", floatCompareOp<CompareEQ>, 2);
    initNativeMethod(cls, "__ne__", floatCompareOp<CompareNE>, 2);
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
