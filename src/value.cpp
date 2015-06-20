#include "value-inl.h"

#include "object.h"

RootArray<Value, 0> EmptyValueArray;

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
    else
        s << *v.asObject();
    return s;
}
