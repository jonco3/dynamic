#ifndef __VALUE_INL_H__
#define __VALUE_INL_H__

#include "value.h"

#include "numeric.h"
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
        gc.trace(t, &o);
        *v = o;
    }
}

void GCTraits<Value>::checkValid(Value value)
{
#ifdef DEBUG
    if (value.isObject()) {
        Object* o = value.asObject();
        if (o)
            o->checkValid();
    }
#endif
}

inline Object* Value::toObject() const {
    if (isObject())
        return asObject();
    else
        return Integer::getObject(asInt32());
}

bool Value::isNone() const
{
    return isObject() && asObject()->isNone();
}

inline bool Value::isTrue() const
{
    return toObject()->isTrue();
}

inline bool Value::isInstanceOf(Traced<Class*> cls) const
{
    return type()->isDerivedFrom(cls);
}

inline bool Value::isInt() const
{
    return isInt32() || asObject()->is<Integer>();
}

inline int32_t Value::toInt32() const
{
    return isInt32() ? asInt32() : as<Integer>()->value();
}

inline Class* Value::type() const
{
    return isInt32() ? Integer::ObjectClass : asObject()->type();
}

template <typename T>
inline bool Value::is() const
{
    return type() == T::ObjectClass;
}

template <typename T>
inline T* Value::as()
{
    Object* obj = toObject();
    assert(obj->isInstanceOf(T::ObjectClass));
    return obj->as<T>();
}

template <typename T>
inline const T* Value::as() const
{
    Object* obj = toObject();
    assert(obj->isInstanceOf(T::ObjectClass));
    return obj->as<T>();
}


inline Value Value::getAttr(Name name) const
{
    Object* obj = isInt32() ? Integer::ObjectClass : asObject();
    return obj->getAttr(name);
}

inline bool Value::maybeGetAttr(Name name, MutableTraced<Value> valueOut) const
{
    Object* obj = isInt32() ? Integer::ObjectClass : asObject();
    return obj->maybeGetAttr(name, valueOut);
}

template <typename W>
inline bool WrapperMixins<W, Value>::isInt() const
{
    return get()->isInt();
}

template <typename W>
inline int32_t WrapperMixins<W, Value>::toInt32() const
{
    return get()->toInt32();
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
inline Object* WrapperMixins<W, Value>::maybeObject() const
{
    return get()->maybeObject();
}

template <typename W>
inline Object* WrapperMixins<W, Value>::toObject() const
{
    return get()->toObject();
}

template <typename W>
inline bool WrapperMixins<W, Value>::isNone() const
{
    return get()->isObject() && get()->asObject() == None;
}

template <typename W>
inline Class* WrapperMixins<W, Value>::type() const
{
    return get()->type();
}

template <typename W>
template <typename T>
inline bool WrapperMixins<W, Value>::is() const
{
    return get()->template is<T>();
}

template <typename W>
template <typename T>
inline T* WrapperMixins<W, Value>::as()
{
    return get()->template as<T>();
}

template <typename W>
inline Value WrapperMixins<W, Value>::getAttr(Name name) const
{
    return get()->getAttr(name);
}

template <typename W>
inline bool WrapperMixins<W, Value>::maybeGetAttr(Name name, MutableTraced<Value> valueOut) const
{
    return get()->maybeGetAttr(name, valueOut);
}

template <typename W>
inline bool WrapperMixins<W, Value>::isTrue() const
{
    return get()->isTrue();
}

template <typename W>
inline bool WrapperMixins<W, Value>::isInstanceOf(Traced<Class*> cls) const
{
    return get()->isInstanceOf(cls);
}

#endif
