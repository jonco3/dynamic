#include "value-inl.h"

#include "object.h"

ostream& operator<<(ostream& s, const Value& v) {
    Object* obj = v.toObject();
    if (obj)
        s << obj;
    else
        s << "(nullptr)";
    return s;
}

void GCTraits<Value>::checkValid(Value value)
{
    if (value.isObject()) {
        Object* o = value.asObject();
        if (o)
            o->checkValid();
    }
}

bool Value::operator==(const Value& other) const
{
    if (isInt32())
        return other.isInt32() && asInt32() == other.asInt32();

    return asObject()->equals(other.toObject());
}

size_t Value::hash() const
{
    if (isInt32())
        return asInt32() * 0x12345678;  // todo: replace with something better

    Root<Object*> obj(toObject());
    size_t result = obj->hash();
    return result;
    // todo: work out how to reenter intpreter and call __hash__().
}
