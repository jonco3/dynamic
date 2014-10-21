#ifndef __VALUE_INL_H__
#define __VALUE_INL_H__

#include "value.h"

#include "object.h"

template <>
struct GCTraits<Value>
{
    static Value nullValue() {
        return Value(nullptr);
    }

    static bool isNonNull(Value value) {
        return value.asObject() != nullptr;
    }

    static void checkValid(Value value);

    static void trace(Tracer& t, Value* v)
    {
        Object* o = v->asObject();
        gc::trace(t, &o);
        *const_cast<Value*>(v) = Value(o);
    }
};

inline bool Value::isTrue() const
{
    return toObject()->isTrue();
}

#endif
