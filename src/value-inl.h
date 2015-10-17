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
        gc.traceUnbarriered(t, &o);
        *v = o;
    }
}

#ifdef DEBUG
void GCTraits<Value>::checkValid(Value value)
{
    if (value.isObject()) {
        Object* o = value.asObject();
        if (o)
            o->checkValid();
    }
}
#endif

inline bool Value::isNaNOrInfinity(uint64_t bits)
{
    return (bits & DoubleExponentMask) == DoubleExponentMask;
}

inline void Value::setDouble(double value) {
    DoublePun p;
    p.value = value;

    // Canonicalise NaNs and infinity.
    if (isNaNOrInfinity(p.bits))
        p.bits = canonicalizeNaN(p.bits);

    bits = p.bits ^ DoubleXorConstant;
}

inline Object* Value::toObject() const {
    if (isObject())
        return asObject();
    else if (isInt32())
        return Integer::getObject(asInt32());
    else
        return Float::getObject(asDouble());
}

bool Value::isNone() const
{
    return isObject() && asObject()->isNone();
}

inline bool Value::isTrue() const
{
    if (isInt32())
        return asInt32() != 0;
    else if (isDouble())
        return asDouble() != 0.0;
    else
        return toObject()->isTrue();
}

inline bool Value::isInstanceOf(Traced<Class*> cls) const
{
    return type()->isDerivedFrom(cls);
}

inline bool Value::isInt() const
{
    return isInt32() || (isObject() && asObject()->isInstanceOf<Integer>());
}

inline bool Value::isFloat() const
{
    return isDouble() || (isObject() && asObject()->isInstanceOf<Float>());
}

inline int64_t Value::toInt() const
{
    assert(isInt());
    return isInt32() ? asInt32() : as<Integer>()->value();
}

inline double Value::toFloat() const
{
    assert(isFloat());
    return isDouble() ? asDouble() : as<Float>()->value();
}

inline Class* Value::type() const
{
    if (isInt32())
        return Integer::ObjectClass;
    else if (isDouble())
        return Float::ObjectClass;
    else
        return asObject()->type();
}

template <typename T>
inline bool Value::is() const
{
    return type() == T::ObjectClass;
}

template <typename T>
inline T* Value::as() const
{
    Object* obj = asObject();
    return obj->as<T>();
}

inline Object* Value::objectOrType() const
{
    if (isInt32())
        return Integer::ObjectClass;
    else if (isDouble())
        return Float::ObjectClass;
    else
        return asObject();
}

inline Value Value::getAttr(Name name) const
{
    return objectOrType()->getAttr(name);
}

inline bool Value::maybeGetAttr(Name name, MutableTraced<Value> valueOut) const
{
    return objectOrType()->maybeGetAttr(name, valueOut);
}

template <typename W>
inline bool WrapperMixins<W, Value>::isObject() const
{
    return extract()->isObject();
}

template <typename W>
inline bool WrapperMixins<W, Value>::isInt32() const
{
    return extract()->isInt32();
}

template <typename W>
inline bool WrapperMixins<W, Value>::isDouble() const
{
    return extract()->isDouble();
}

template <typename W>
inline bool WrapperMixins<W, Value>::isInt() const
{
    return extract()->isInt();
}

template <typename W>
inline bool WrapperMixins<W, Value>::isFloat() const
{
    return extract()->isFloat();
}

template <typename W>
inline int64_t WrapperMixins<W, Value>::toInt() const
{
    return extract()->toInt();
}

template <typename W>
inline double WrapperMixins<W, Value>::toFloat() const
{
    return extract()->toFloat();
}

template <typename W>
inline Object* WrapperMixins<W, Value>::asObject() const
{
    return extract()->asObject();
}

template <typename W>
inline Object* WrapperMixins<W, Value>::maybeObject() const
{
    return extract()->maybeObject();
}

template <typename W>
inline Object* WrapperMixins<W, Value>::toObject() const
{
    return extract()->toObject();
}

template <typename W>
inline bool WrapperMixins<W, Value>::isNone() const
{
    return extract()->isObject() && extract()->asObject() == None;
}

template <typename W>
inline Class* WrapperMixins<W, Value>::type() const
{
    return extract()->type();
}

template <typename W>
template <typename T>
inline bool WrapperMixins<W, Value>::is() const
{
    return extract()->template is<T>();
}

template <typename W>
template <typename T>
inline T* WrapperMixins<W, Value>::as() const
{
    return extract()->template as<T>();
}

template <typename W>
inline Value WrapperMixins<W, Value>::getAttr(Name name) const
{
    return extract()->getAttr(name);
}

template <typename W>
inline bool WrapperMixins<W, Value>::maybeGetAttr(Name name, MutableTraced<Value> valueOut) const
{
    return extract()->maybeGetAttr(name, valueOut);
}

template <typename W>
inline bool WrapperMixins<W, Value>::isTrue() const
{
    return extract()->isTrue();
}

template <typename W>
inline bool WrapperMixins<W, Value>::isInstanceOf(Traced<Class*> cls) const
{
    return extract()->isInstanceOf(cls);
}

#endif
