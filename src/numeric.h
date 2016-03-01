#ifndef __NUMERIC_H__
#define __NUMERIC_H__

#include "exception.h"
#include "object.h"

#include <gmpxx.h>

extern bool logBigInt;

struct Integer : public Object
{
    static void init();
    static void final();

    static GlobalRoot<Class*> ObjectClass;

    static Value get(int64_t v) {
        int32_t int32value = v;
        if (int32value == v)
            return Value(int32value);

        return getObject(v);
    }

    static Value get(mpz_class&& v) {
        static_assert(sizeof(long) >= sizeof(int32_t),
                      "This assumes that get_si() returns enough bits");

        int32_t int32value = v.get_si();
        if (int32value == v)
            return Value(int32value);

        return getObject(move(v));
    }

    static Value get(const string& s);

    static Object* getObject(int64_t v);
    static Object* getObject(mpz_class&& v);

    Integer(int64_t v = 0);
    Integer(mpz_class&& v);
    Integer(Traced<Class*> cls, int64_t v);

    const mpz_class& value() const { return value_; }

    template <class T>
    bool toSigned(T& out) {
        static_assert(sizeof(value_.get_si()) >= sizeof(T),
                      "get_si() doesn't return enough bits");

        out = T(value_.get_si());
        return out == value_;
    }

    template <class T>
    bool toUnsigned(T& out) {
        static_assert(sizeof(value_.get_ui()) >= sizeof(T),
                      "get_ui() doesn't return enough bits");

        out = T(value_.get_ui());
        return out == value_;
    }

    void traceChildren(Tracer& t) override;
    void print(ostream& s) const override;

    template <BinaryOp Op>
    static bool binaryOp(int32_t a, int32_t b, MutableTraced<Value> resultOut);

    template <BinaryOp Op>
    static bool binaryOp(const mpz_class& a, const mpz_class& b,
                         MutableTraced<Value> resultOut);

    template <BinaryOp Op>
    static bool binaryOp(const mpz_class& a, int32_t b, MutableTraced<Value> resultOut);

    template <BinaryOp Op>
    static bool binaryOp(int32_t a, const mpz_class& b, MutableTraced<Value> resultOut);

    template <CompareOp Op>
    static Value compareOp(int32_t a, int32_t b);

    template <CompareOp Op>
    static Value compareOp(const mpz_class& a, const mpz_class& b);

    template <CompareOp Op>
    static Value compareOp(const mpz_class& a, int32_t b);

    template <CompareOp Op>
    static Value compareOp(int32_t a, const mpz_class& b);

  private:
    mpz_class value_;

    static bool largeLeftShift(int32_t a, int32_t b, MutableTraced<Value> resultOut);
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

    return largeLeftShift(a, b, resultOut);
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
