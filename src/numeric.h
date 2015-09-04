#ifndef __NUMERIC_H__
#define __NUMERIC_H__

#include "bool.h"
#include "object.h"

struct Integer : public Object
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    static Value get(int64_t v) {
        int32_t i32 = static_cast<int32_t>(v);
        if (i32 == v)
            return Value(i32);
        else
            return getObject(v);
    }

    static Object* getObject(int64_t v);

    Integer(int64_t v = 0);
    Integer(Traced<Class*> cls);

    int64_t value() const { return value_; }
    void print(ostream& s) const override;

    template <BinaryOp Op>
    static inline Value binaryOp(int64_t a, int64_t b);

    template <CompareOp Op>
    static inline Value compareOp(int64_t a, int64_t b);

  private:
    int64_t value_;
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

  private:
    const double value_;
};

template <> inline Value Integer::binaryOp<BinaryAdd>(int64_t a, int64_t b)
{
    return Integer::get(a + b);
}

template <> inline Value Integer::binaryOp<BinaryMinus>(int64_t a, int64_t b)
{
    return Integer::get(a - b);
}

template <> inline Value Integer::binaryOp<BinaryMultiply>(int64_t a, int64_t b)
{
    return Integer::get(a * b);
}

template <> inline Value Integer::binaryOp<BinaryTrueDiv>(int64_t a, int64_t b)
{
    return Float::get(double(a) / b);
}

template <> inline Value Integer::binaryOp<BinaryFloorDiv>(int64_t a, int64_t b)
{
    return Integer::get(a / b);
}

template <> inline Value Integer::binaryOp<BinaryModulo>(int64_t a, int64_t b)
{
    return Integer::get(a % b);
}

template <> inline Value Integer::binaryOp<BinaryPower>(int64_t a, int64_t b)
{
    return Integer::get((int)pow(a, b));
}

template <> inline Value Integer::binaryOp<BinaryOr>(int64_t a, int64_t b)
{
    return Integer::get(a | b);
}

template <> inline Value Integer::binaryOp<BinaryXor>(int64_t a, int64_t b)
{
    return Integer::get(a ^ b);
}

template <> inline Value Integer::binaryOp<BinaryAnd>(int64_t a, int64_t b)
{
    return Integer::get(a & b);
}

template <> inline Value Integer::binaryOp<BinaryLeftShift>(int64_t a, int64_t b)
{
    return Integer::get(a << b);
}

template <> inline Value Integer::binaryOp<BinaryRightShift>(int64_t a, int64_t b)
{
    return Integer::get(a >> b);
}

template <> inline Value Integer::compareOp<CompareLT>(int64_t a, int64_t b)
{
    return Boolean::get(a < b);
}

template <> inline Value Integer::compareOp<CompareLE>(int64_t a, int64_t b)
{
    return Boolean::get(a <= b);
}

template <> inline Value Integer::compareOp<CompareGT>(int64_t a, int64_t b)
{
    return Boolean::get(a > b);
}

template <> inline Value Integer::compareOp<CompareGE>(int64_t a, int64_t b)
{
    return Boolean::get(a >= b);
}

template <> inline Value Integer::compareOp<CompareEQ>(int64_t a, int64_t b)
{
    return Boolean::get(a == b);
}

template <> inline Value Integer::compareOp<CompareNE>(int64_t a, int64_t b)
{
    return Boolean::get(a != b);
}

#endif
