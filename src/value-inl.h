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
    if (v->isObject()) {
        Object* o = v->asObject();
        gc::trace(t, &o);
        *const_cast<Value*>(v) = Value(o);
    }
}

inline Value::Value(int16_t i)
  : bits((i << PayloadShift) | IntType)
{
    assert(i >= INT16_MIN && i <= INT16_MAX);
}

inline Object* Value::toObject() const {
    if (isObject())
        return asObject();
    else
        return Integer::getObject(asInt32());
}

inline bool Value::isTrue() const
{
    return toObject()->isTrue();
}

inline bool Value::isInt32() const
{
    if (type() == IntType)
        return true;
    Object* obj = asObject();
    if (!obj)
        return false;
    return obj->is<Integer>();
}

inline int32_t Value::asInt32() const
{
    if (type() == IntType)
        return payload();
    else
        return asObject()->as<Integer>()->value();
}

inline bool Value::maybeGetAttr(Name name, Value& valueOut)
{
    if (type() == IntType)
        return Integer::Proto->maybeGetAttr(name, valueOut);
    else
        return asObject()->maybeGetAttr(name, valueOut);
}

inline bool TracedMixins<Value>::isObject() const
{
    return get()->isObject();
}

inline Object* TracedMixins<Value>::asObject() const
{
    return get()->asObject();
}

inline Object* TracedMixins<Value>::toObject() const
{
    return get()->toObject();
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
