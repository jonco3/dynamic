#ifndef __NUMERIC_H__
#define __NUMERIC_H__

#include "exception.h"
#include "object.h"

#include <gmpxx.h>

struct Integer : public Object
{
    static void init();
    static void final();

    static GlobalRoot<Class*> ObjectClass;

    template <typename T>
    static bool fitsInt32(T&& v) {
        return v >= INT32_MIN && v <= INT32_MAX;
    }

    static Value get(int64_t v) {
        if (fitsInt32(v))
            return Value(int32_t(v));

        return getObject(v);
    }

    static Value get(const mpz_class& v) {
        static_assert(sizeof(long) >= sizeof(int32_t),
                      "This assumes that get_si() returns enough bits");

        if (fitsInt32(v))
            return Value(int32_t(v.get_si()));

        return getObject(v);
    }

    static Value get(const string& s);

    static Object* getObject(int64_t v);
    static Object* getObject(const mpz_class& v);

    Integer(int64_t v = 0);
    Integer(const mpz_class& v);
    Integer(Traced<Class*> cls, int64_t v);

    const mpz_class& value() const { return value_; }

    void traceChildren(Tracer& t) override;
    void print(ostream& s) const override;

    template <BinaryOp Op>
    static bool binaryOp(int32_t a, int32_t b, MutableTraced<Value> resultOut);

    template <CompareOp Op>
    static Value compareOp(int32_t a, int32_t b);

  private:
    mpz_class value_;
};

struct Boolean : public Integer
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    static GlobalRoot<Boolean*> True;
    static GlobalRoot<Boolean*> False;

    static Boolean* get(bool v) { return v ? True : False; }

    bool boolValue() {
        assert(this == True || this == False);
        return this == True;
    }

    void print(ostream& s) const override;

    Boolean(int64_t value);

  private:
    static bool ClassInitialised;
};

struct Float : public Object
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    static Value get(double v);
    static Object* getObject(double v);

    Float(double v = 0.0);
    Float(Traced<Class*> cls);

    double value() const { return value_; }
    void print(ostream& s) const override;

    template <BinaryOp Op>
    static inline Value binaryOp(double a, double b);

    template <CompareOp Op>
    static inline Value compareOp(double a, double b);

  private:
    const double value_;
};

template <>
inline bool
Integer::binaryOp<BinaryAdd>(int32_t a, int32_t b, MutableTraced<Value> resultOut)
{
    resultOut = Integer::get(int64_t(a) + b);
    return true;
}

template <>
inline bool
Integer::binaryOp<BinarySub>(int32_t a, int32_t b, MutableTraced<Value> resultOut)
{
    resultOut = Integer::get(int64_t(a) - b);
    return true;
}

template <>
inline bool
Integer::binaryOp<BinaryMul>(int32_t a, int32_t b, MutableTraced<Value> resultOut)
{
    resultOut = Integer::get(int64_t(a) * b);
    return true;
}

template <>
inline bool
Integer::binaryOp<BinaryTrueDiv>(int32_t a, int32_t b, MutableTraced<Value> resultOut)
{
    resultOut = Float::get(double(a) / b);
    return true;
}

template <>
inline bool
Integer::binaryOp<BinaryFloorDiv>(int32_t a, int32_t b, MutableTraced<Value> resultOut)
{
    resultOut = Integer::get(a / b);
    return true;
}

template <>
inline bool
Integer::binaryOp<BinaryModulo>(int32_t a, int32_t b, MutableTraced<Value> resultOut)
{
    resultOut = Integer::get(a % b);
    return true;
}

template <>
inline bool
Integer::binaryOp<BinaryOr>(int32_t a, int32_t b, MutableTraced<Value> resultOut)
{
    resultOut = Integer::get(a | b);
    return true;
}

template <>
inline bool
Integer::binaryOp<BinaryXor>(int32_t a, int32_t b, MutableTraced<Value> resultOut)
{
    resultOut = Integer::get(a ^ b);
    return true;
}

template <>
inline bool
Integer::binaryOp<BinaryAnd>(int32_t a, int32_t b, MutableTraced<Value> resultOut)
{
    resultOut = Integer::get(a & b);
    return true;
}

template <>
inline bool
Integer::binaryOp<BinaryLeftShift>(int32_t a, int32_t b, MutableTraced<Value> resultOut)
{
    if (b < 0) {
        resultOut = gc.create<ValueError>("negative shift count");
        return false;
    }

    if (b < 32) {
        resultOut = Integer::get(int64_t(a) << b);
        return true;
    }

    AutoSupressGC supressGC;
    mpz_t aa;
    mpz_init_set_si(aa, a);
    mpz_class r;
    mpz_mul_2exp(r.get_mpz_t(), aa, b);
    resultOut = Integer::get(r);
    return true;
}

template <>
inline bool
Integer::binaryOp<BinaryRightShift>(int32_t a, int32_t b, MutableTraced<Value> resultOut)
{
    if (b < 0) {
        resultOut = gc.create<ValueError>("negative shift count");
        return false;
    }

    resultOut = Integer::get(int64_t(a) >> b);
    return true;
}

template <> inline Value Integer::compareOp<CompareLT>(int32_t a, int32_t b)
{
    return Boolean::get(a < b);
}

template <> inline Value Integer::compareOp<CompareLE>(int32_t a, int32_t b)
{
    return Boolean::get(a <= b);
}

template <> inline Value Integer::compareOp<CompareGT>(int32_t a, int32_t b)
{
    return Boolean::get(a > b);
}

template <> inline Value Integer::compareOp<CompareGE>(int32_t a, int32_t b)
{
    return Boolean::get(a >= b);
}

template <> inline Value Integer::compareOp<CompareEQ>(int32_t a, int32_t b)
{
    return Boolean::get(a == b);
}

template <> inline Value Integer::compareOp<CompareNE>(int32_t a, int32_t b)
{
    return Boolean::get(a != b);
}

template <> inline Value Float::binaryOp<BinaryAdd>(double a, double b)
{
    return Float::get(a + b);
}

template <> inline Value Float::binaryOp<BinarySub>(double a, double b)
{
    return Float::get(a - b);
}

template <> inline Value Float::binaryOp<BinaryMul>(double a, double b)
{
    return Float::get(a * b);
}

template <> inline Value Float::binaryOp<BinaryTrueDiv>(double a, double b)
{
    return Float::get(a / b);
}

template <> inline Value Float::binaryOp<BinaryFloorDiv>(double a, double b)
{
    return Float::get(floor(a / b));
}

template <> inline Value Float::binaryOp<BinaryModulo>(double a, double b)
{
    return Float::get(fmod(a, b));
}

template <> inline Value Float::binaryOp<BinaryPower>(double a, double b)
{
    return Float::get(pow(a, b));
}

template <> inline Value Float::compareOp<CompareLT>(double a, double b)
{
    return Boolean::get(a < b);
}

template <> inline Value Float::compareOp<CompareLE>(double a, double b)
{
    return Boolean::get(a <= b);
}

template <> inline Value Float::compareOp<CompareGT>(double a, double b)
{
    return Boolean::get(a > b);
}

template <> inline Value Float::compareOp<CompareGE>(double a, double b)
{
    return Boolean::get(a >= b);
}

template <> inline Value Float::compareOp<CompareEQ>(double a, double b)
{
    return Boolean::get(a == b);
}

template <> inline Value Float::compareOp<CompareNE>(double a, double b)
{
    return Boolean::get(a != b);
}

#endif
