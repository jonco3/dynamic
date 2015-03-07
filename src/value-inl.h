#ifndef __VALUE_INL_H__
#define __VALUE_INL_H__

#include "value.h"

#include "integer.h"
#include "object.h"

inline Value GCTraits<Value>::nullValue() {
    return Value();
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

inline Class* Value::getType() const
{
    if (type() == IntType)
        return Integer::ObjectClass;
    else
        return asObject()->getType();
}

inline bool Value::maybeGetAttr(Name name, Root<Value>& valueOut) const
{
    if (type() == IntType)
        return Integer::Proto->maybeGetAttr(name, valueOut);
    else
        return asObject()->maybeGetAttr(name, valueOut);
}

template <typename W>
inline bool WrapperMixins<W, Value>::isObject() const
{
    return get()->isObject();
}

template <typename W>
inline Object* WrapperMixins<W, Value>::asObject() const
{
    return get()->asObject();
}

template <typename W>
inline Object* WrapperMixins<W, Value>::toObject() const
{
    return get()->toObject();
}

template <typename W>
inline bool WrapperMixins<W, Value>::isInt32() const
{
    return get()->isInt32();
}

template <typename W>
inline int32_t WrapperMixins<W, Value>::asInt32() const
{
    return get()->asInt32();
}

template <typename W>
inline Class* WrapperMixins<W, Value>::getType() const
{
    return get()->getType();
}

template <typename W>
inline bool WrapperMixins<W, Value>::maybeGetAttr(Name name, Root<Value>& valueOut) const
{
    return get()->maybeGetAttr(name, valueOut);
}

#endif
