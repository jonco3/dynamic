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

bool Value::toInt32(int32_t& out) const
{
    assert(isInt());
    if (isInt32()) {
        out = asInt32();
        return true;
    }

    return as<Integer>()->toSigned(out);
}

bool Value::toSize(size_t& out) const
{
    assert(isInt());
    if (isInt32()) {
        int32_t i = asInt32();
        if (i < 0)
            return false;

        out = asInt32();
        return true;
    }

    return as<Integer>()->toUnsigned(out);
}
