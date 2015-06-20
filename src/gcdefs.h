#ifndef __GCDEFS_H__
#define __GCDEFS_H__

#include "gc.h"
#include "name.h"

struct Class;
struct Object;
struct Value;

template <>
struct GCTraits<Value>
{
    static inline Value nullValue();
    static inline bool isNonNull(Value value);
    static inline void checkValid(Value value);
    static inline void trace(Tracer& t, Value* v);
};

template <typename W>
struct WrapperMixins<W, Value>
{
    inline bool isInt() const;
    inline int32_t toInt32() const;
    inline bool isObject() const;
    inline Object *asObject() const;
    inline Object *maybeObject() const;
    inline Object *toObject() const;
    inline bool isNone() const;
    inline Value getAttr(Name name) const;
    inline bool maybeGetAttr(Name name, MutableTraced<Value> valueOut) const;
    inline Class* type() const;
    template <typename T> inline bool is() const;
    template <typename T> inline T* as();
    inline bool isTrue() const;
    inline bool isInstanceOf(Traced<Class*> cls) const;

  private:
    const Value* get() const {
        return static_cast<const W*>(this)->location();
    }
};

template <typename W, typename T>
struct WrapperMixins<W, T*>
{
    operator Traced<Value> () const {
        static_assert(is_base_of<Object, T>::value,
                      "T must derive from object for this conversion");
        // Since Traced<T> is immutable and all Objects are Values, we can
        // safely cast a Stack<Object*> to a Traced<Value>.
        const Value* ptr = reinterpret_cast<Value const*>(get());
        return Traced<Value>::fromTracedLocation(ptr);
    }

    operator Traced<Object*> () const {
        static_assert(is_base_of<Object, T>::value,
                      "T must derive from object for this conversion");
        Object* const * ptr = reinterpret_cast<Object* const *>(get());
        return Traced<Object*>::fromTracedLocation(ptr);
    }

  private:
    T* const * get() const {
        return static_cast<W const *>(this)->location();
    }
};

extern RootArray<Value, 0> EmptyValueArray;

#endif
