#include "numeric.h"

#include "callable.h"
#include "exception.h"
#include "singletons.h"
#include "string.h"
#include "value-inl.h"

#include <cmath>
#include <sstream>

//#define TRACE_GMP_ALLOC

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

    if (args[0].isInt32()) {
        resultOut = Integer::get(int32op(args[0].asInt32()));
        return true;
    }

    AutoSupressGC supressGC;
    resultOut = Integer::get(mpzOp(args[0].as<Integer>()->value()));
    return true;
}

template <>
/* static */ bool Integer::binaryOp<BinaryPower>(int32_t a, int32_t b,
                                                 MutableTraced<Value> resultOut)
{
    AutoSupressGC supressGC;
    if (b == 0) {
        resultOut = Value(1);
        return true;
    }

    if (b < 0) {
        resultOut = Float::get(pow(a, b));
        return true;
    }

    mpz_t aa;
    mpz_init_set_si(aa, a);
    mpz_class r;
    mpz_pow_ui(r.get_mpz_t(), aa, b);
    resultOut = Integer::get(r);
    return true;
}

template <BinaryOp Op>
static bool mpzBinaryOp(const mpz_class& a, const mpz_class& b,
                        MutableTraced<Value> resultOut);

template <BinaryOp Op>
static bool mpzIntBinaryOp(const mpz_class& a, int32_t b,
                           MutableTraced<Value> resultOut);

template <BinaryOp Op>
static bool intMpzBinaryOp(int32_t a, const mpz_class& b,
                           MutableTraced<Value> resultOut);

template <BinaryOp Op>
static inline bool mpzBinaryOp(const mpz_class& a, const mpz_class& b,
                               MutableTraced<Value> resultOut)
{
    assert(b.fits_sint_p()); // todo: raise error
    return mpzIntBinaryOp<Op>(a, b.get_si(), resultOut);
}

template <BinaryOp Op>
static inline bool mpzIntBinaryOp(const mpz_class& a, int32_t b,
                                  MutableTraced<Value> resultOut)
{
    mpz_class bb(b);
    return mpzBinaryOp<Op>(a, bb, resultOut);
}

template <BinaryOp Op>
static inline bool intMpzBinaryOp(int32_t a, const mpz_class& b,
                                  MutableTraced<Value> resultOut)
{
    mpz_class aa(a);
    return mpzBinaryOp<Op>(aa, b, resultOut);
}

#define define_binary_op_mpz_mpz(op, expr)                                    \
    template <> inline bool                                                   \
    mpzBinaryOp<op>(const mpz_class& a, const mpz_class& b,                   \
                    MutableTraced<Value> resultOut)                           \
    {                                                                         \
        resultOut = expr;                                                     \
        return true;                                                          \
    }

#define define_binary_op_mpz_int(op, expr)                                    \
    template <> inline bool                                                   \
    mpzIntBinaryOp<op>(const mpz_class& a, int32_t b,                         \
                       MutableTraced<Value> resultOut)                        \
    {                                                                         \
        resultOut = expr;                                                     \
        return true;                                                          \
    }

#define define_binary_op_int_mpz(op, expr)                                    \
    template <> inline bool                                                   \
    intMpzBinaryOp<op>(int32_t a, const mpz_class& b,                         \
                       MutableTraced<Value> resultOut)                        \
    {                                                                         \
        resultOut = expr;                                                     \
        return true;                                                          \
    }

#define define_binary_ops(op, expr)                                           \
    define_binary_op_mpz_mpz(op, Integer::get(expr))                          \
    define_binary_op_mpz_int(op, Integer::get(expr))                          \
    define_binary_op_int_mpz(op, Integer::get(expr))

define_binary_ops(BinaryAdd, a + b)
define_binary_ops(BinarySub, a - b)
define_binary_ops(BinaryMul, a * b)
define_binary_ops(BinaryFloorDiv, a / b)

define_binary_op_mpz_mpz(BinaryTrueDiv, Float::get(a.get_d() / b.get_d()))
define_binary_op_mpz_int(BinaryTrueDiv, Float::get(a.get_d() / b))
define_binary_op_int_mpz(BinaryTrueDiv, Float::get(double(a) / b.get_d()))

define_binary_ops(BinaryModulo, a % b)

template <>
inline bool mpzIntBinaryOp<BinaryPower>(const mpz_class& a, int32_t b,
                                        MutableTraced<Value> resultOut)
{
    if (b == 0) {
        resultOut = Integer::get(1);
        return true;
    }

    if (b < 0) {
        resultOut = Float::get(pow(a.get_d(), b));
        return true;
    }

    mpz_class r;
    mpz_pow_ui(r.get_mpz_t(), a.get_mpz_t(), b);
    resultOut = Integer::get(r);
    return true;
}

define_binary_ops(BinaryOr, a | b)
define_binary_ops(BinaryXor, a ^ b)
define_binary_ops(BinaryAnd, a & b)

template <>
inline bool mpzIntBinaryOp<BinaryLeftShift>(const mpz_class& a, int32_t b,
                                            MutableTraced<Value> resultOut)
{
    if (b < 0) {
        resultOut = gc.create<ValueError>("negative shift count");
        return false;
    }

    mpz_class r;
    mpz_mul_2exp(r.get_mpz_t(), a.get_mpz_t(), b);
    resultOut = Integer::get(r);
    return true;
}

template <>
inline bool mpzIntBinaryOp<BinaryRightShift>(const mpz_class& a, int32_t b,
                                             MutableTraced<Value> resultOut)
{
    if (b < 0) {
        resultOut = gc.create<ValueError>("negative shift count");
        return false;
    }

    mpz_class r;
    mpz_div_2exp(r.get_mpz_t(), a.get_mpz_t(), b);
    // todo: rounds towards zero
    resultOut = Integer::get(r);
    return true;
}

template <CompareOp Op>
static Value mpzCompareOp(const mpz_class& a, const mpz_class& b);

template <CompareOp Op>
static Value mpzIntCompareOp(const mpz_class& a, int32_t b);

template <CompareOp Op>
static Value intMpzCompareOp(int32_t a, const mpz_class& b);

#define define_compare_op_mpz_mpz(op, expr)                                    \
    template <> inline Value                                                  \
    mpzCompareOp<op>(const mpz_class& a, const mpz_class& b)                   \
    {                                                                         \
        return expr;                                                          \
    }

#define define_compare_op_mpz_int(op, expr)                                    \
    template <> inline Value                                                  \
    mpzIntCompareOp<op>(const mpz_class& a, int32_t b)                         \
    {                                                                         \
        return expr;                                                          \
    }

#define define_compare_op_int_mpz(op, expr)                                    \
    template <> inline Value                                                  \
    intMpzCompareOp<op>(int32_t a, const mpz_class& b)                         \
    {                                                                         \
        return expr;                                                          \
    }

#define define_compare_ops(op, expr)                                           \
    define_compare_op_mpz_mpz(op, Boolean::get(expr))                          \
    define_compare_op_mpz_int(op, Boolean::get(expr))                          \
    define_compare_op_int_mpz(op, Boolean::get(expr))

define_compare_ops(CompareLT, a < b)
define_compare_ops(CompareLE, a <= b)
define_compare_ops(CompareGT, a > b)
define_compare_ops(CompareGE, a >= b)
define_compare_ops(CompareEQ, a == b)
define_compare_ops(CompareNE, a != b)

template <BinaryOp Op>
static bool intBinaryOp(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    if (!args[0].isInstanceOf(Integer::ObjectClass) ||
        !args[1].isInstanceOf(Integer::ObjectClass))
    {
        resultOut = NotImplemented;
        return true;
    }

    bool leftIsInt32 = args[0].isInt32();
    bool rightIsInt32 = args[1].isInt32();
    if (leftIsInt32 && rightIsInt32) {
        return Integer::binaryOp<Op>(args[0].asInt32(), args[1].asInt32(),
                                     resultOut);
    }

    AutoSupressGC supressGC;

    if (leftIsInt32) {
        return intMpzBinaryOp<Op>(args[0].asInt32(),
                                  args[1].as<Integer>()->value(),
                                  resultOut);
    }

    if (rightIsInt32) {
        return mpzIntBinaryOp<Op>(args[0].as<Integer>()->value(),
                                  args[1].asInt32(),
                                  resultOut);
    }

    return mpzBinaryOp<Op>(args[0].as<Integer>()->value(),
                           args[1].as<Integer>()->value(),
                           resultOut);
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

    bool leftIsInt32 = args[0].isInt32();
    bool rightIsInt32 = args[1].isInt32();
    if (leftIsInt32 && rightIsInt32) {
        resultOut =
            Integer::compareOp<Op>(args[0].asInt32(), args[1].asInt32());
        return true;
    }

    if (leftIsInt32) {
        resultOut = intMpzCompareOp<Op>(args[0].asInt32(),
                                       args[1].as<Integer>()->value());
        return true;
    }

    if (rightIsInt32) {
        resultOut = mpzIntCompareOp<Op>(args[0].as<Integer>()->value(),
                                       args[1].asInt32());
        return true;
    }

    resultOut = mpzCompareOp<Op>(args[0].as<Integer>()->value(),
                                args[1].as<Integer>()->value());
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

struct GMPData : public Cell
{
    GMPData() {
#ifdef DEBUG
        magic = GMPData::MagicValue;
#endif
    }

    static GMPData* fromPtr(void* data);
    static GMPData* fromPtrUnchecked(void* data);

    void* toPtr() {
        return &data;
    }

#ifdef DEBUG
    void markFree() {
        magic = 0;
    }
#endif

#ifdef DEBUG
    static const uint32_t MagicValue = 0x2ac52ac5;
    uint32_t magic;
#endif

  private:
    char data[0];
};

/* static */ GMPData* GMPData::fromPtrUnchecked(void* data) {
    static const size_t offset = size_t(&reinterpret_cast<GMPData*>(0)->data);

    auto addr = reinterpret_cast<uintptr_t>(data) - offset;
    return reinterpret_cast<GMPData*>(addr);
}

/* static */ GMPData* GMPData::fromPtr(void* data) {
    GMPData* cell = fromPtrUnchecked(data);
    assert(cell->magic == GMPData::MagicValue);
    return cell;
}

static void* AllocGMPData(size_t bytes)
{
#ifdef TRACE_GMP_ALLOC
    printf("AllocGMPData %lu\n", bytes);
#endif
    GMPData* cell = gc.createSized<GMPData>(sizeof(GMPData) + bytes);
    return cell->toPtr();
}

static void FreeGMPData(void* data, size_t bytes)
{
#ifdef DEBUG
    if (!gc.currentlySweeping()) {
        GMPData::fromPtr(data)->markFree();
        return;
    }

    GMPData* cell = GMPData::fromPtrUnchecked(data);
    assert(cell->magic == GMPData::MagicValue || cell->magic == 0xffffffff);
    if (cell->magic == GMPData::MagicValue)
        cell->markFree();
#endif
}

static void* ReallocGMPData(void* oldData, size_t oldBytes, size_t newBytes)
{
#ifdef TRACE_GMP_ALLOC
    printf("ReallocGMPData %lu %lu\n", oldBytes, newBytes);
#endif
    GMPData::fromPtr(oldData);
    if (newBytes <= oldBytes)
        return oldData;

    void* newData = AllocGMPData(newBytes);
    memcpy(newData, oldData, oldBytes);
    FreeGMPData(oldData, oldBytes);
    return newData;
}

void Integer::traceChildren(Tracer& t)
{
    mpz_t* mpz = reinterpret_cast<mpz_t*>(&value_);
    mp_limb_t** pptr = &(*mpz)[0]._mp_d;
    if (!*pptr)
        return;

    GMPData* cell = GMPData::fromPtr(*pptr);
    gc.traceUnbarriered(t, &cell);
    *pptr = static_cast<mp_limb_t *>(cell->toPtr());
}

void Integer::init()
{
    mp_set_memory_functions(AllocGMPData, ReallocGMPData, FreeGMPData);

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
    AutoSupressGC supressGC;
    size_t bits = (int32_t(i) == i) ? 32 : 64;
    mpz_t n;
    mpz_init2(n, bits + sizeof(mp_limb_t) * 8);
    mpz_set_si64(n, i);
    mpz_class r(n);
    mpz_clear(n);
    return r;
}

inline Integer::Integer(int64_t v)
  : Object(ObjectClass), value_(mpzFromInt64(v))
{}

inline Integer::Integer(const mpz_class& v)
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

/* static */ Value Integer::get(const string& s)
{
    // todo: this probably doesn't correctly parse all python integers
    AutoSupressGC supressGC;
    mpz_class i(s);
    return get(i);
}

void Integer::print(ostream& s) const {
    AutoSupressGC supressGC;
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
