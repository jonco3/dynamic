#ifndef __VALUE_INL_H__
#define __VALUE_INL_H__

#include "value.h"

#include "integer.h"
#include "object.h"

inline Value GCTraits<Value>::nullValue() {
    return Value(nullptr);
}

inline bool GCTraits<Value>::isNonNull(Value value) {
    return value.asObject() != nullptr;
}

inline void GCTraits<Value>::trace(Tracer& t, Value* v)
{
    Object* o = v->asObject();
    gc::trace(t, &o);
    *const_cast<Value*>(v) = Value(o);
}

inline bool Value::isTrue() const
{
    return toObject()->isTrue();
}

inline bool Value::isInt32() const
{
    return toObject()->is<Integer>();
}

inline int32_t Value::asInt32() const
{
    return toObject()->as<Integer>()->value();
}

inline bool TracedMixins<Value>::isInt32() const
{
    return get()->isInt32();
}

inline int32_t TracedMixins<Value>::asInt32() const
{
    return get()->asInt32();
}

#endif
