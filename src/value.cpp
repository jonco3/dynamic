#include "value-inl.h"

#include "object.h"

RootVector<Value> EmptyValueArray(0);

uint64_t Value::canonicalizeNaN(uint64_t bits)
{
    uint64_t mantissa = (bits & DoubleMantissaMask);
    uint64_t rest = (bits & ~DoubleMantissaMask);
    mantissa = mantissa != 0 ? 1 : 0;
    return rest | mantissa;
}

ostream& operator<<(ostream& s, const Value& v) {
    if (v.isInt32())
        s << v.asInt32();
    else if (v.isDouble())
        s << v.asDouble();
    else if (v.asObject())
        s << *v.asObject();
    else
        s << "nullptr";
    return s;
}

bool Value::toInt32(int32_t* out) const
{
    static_assert(sizeof(long) >= sizeof(int32_t),
                  "This assumes that get_si() returns enough bits");

    assert(isInt());
    if (isInt32()) {
        *out = asInt32();
        return true;
    }

    mpz_class v = as<Integer>()->value();
    if (v < INT32_MIN || v > INT32_MAX)
        return false;

    assert(v.fits_sint_p());
    *out = v.get_si();
    return true;
}
