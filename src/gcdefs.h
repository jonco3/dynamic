#ifndef __GCDEFS_H__
#define __GCDEFS_H__

#include "gc.h"
#include "name.h"

#include <gmpxx.h>

struct Class;
struct Frame;
struct Layout;
struct Object;
struct Value;

template <>
struct GCTraits<Value>
{
    using BaseType = Value;
    static inline Value nullValue();
    static inline bool isNonNull(Value value);
#ifdef DEBUG
    static inline void checkValid(Value value);
#endif
    static inline void trace(Tracer& t, Value* v);
};

template <>
struct GCTraits<Frame>
{
    static void trace(Tracer& t, Frame* frame);
#ifdef DEBUG
    static void checkValid(const Frame& frame);
#endif
};

template <typename W>
struct WrapperMixins<W, Value>
{
    inline bool isObject() const;
    inline bool isInt32() const;
    inline bool isDouble() const;
    inline bool isInt() const;
    inline bool isFloat() const;
    inline int32_t asInt32() const;
    inline double asDouble() const;
    inline bool toInt32(int32_t& out) const;
    inline bool toSize(size_t& out) const;
    inline double toFloat() const;
    inline Object *asObject() const;
    inline Object *maybeObject() const;
    inline Object *toObject() const;
    inline bool isNone() const;
    inline Value getAttr(Name name) const;
    inline bool maybeGetAttr(Name name, MutableTraced<Value> valueOut) const;
    inline Class* type() const;
    template <typename T> inline bool is() const;
    template <typename T> inline T* as() const;
    inline bool isInstanceOf(Traced<Class*> cls) const;

    template <typename T> bool isInstanceOf() const {
        return isInstanceOf(T::ObjectClass);
    }

  private:
    const Value* extract() const {
        return &static_cast<const W*>(this)->get();
    }
};

template <typename W, typename T>
struct WrapperMixins<W, T*>
{
    operator Traced<Value> () const;

    template <typename S>
    operator Traced<S*> () const;

  private:
    T* const * extract() const {
        return &static_cast<W const *>(this)->get();
    }
};

extern RootVector<Value> EmptyValueArray;

#endif
