#ifndef __VALUE_H__
#define __VALUE_H__

#include "gc.h"
#include "name.h"

#include <ostream>

using namespace std;

struct Class;
struct Integer;
struct Object;
struct Tracer;

struct Value
{
    Value() : objectp(nullptr) {}
    Value(Object* o) : objectp(o) {}
    Value(int16_t i) { *this = i; }

    Value& operator=(Object* const & o) {
        objectp = o;
        return *this;
    }

    Value& operator=(int16_t i) {
        assert(i >= INT16_MIN && i <= INT16_MAX);
        bits = (i << PayloadShift) | IntKind;
        return *this;
    }

    bool isObject() const {
        return kind() == ObjectKind;
    }

    Object *asObject() const {
        assert(isObject());
        return objectp;
    }

    inline bool isNone() const;
    inline Object *toObject() const;
    inline bool isInt32() const;
    inline int32_t asInt32() const;

    inline Class* type() const;
    inline Value getAttr(Name name) const;
    inline bool maybeGetAttr(Name name, Root<Value>& valueOut) const;

    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const { return !(*this == other); }

    size_t hash() const;

    inline bool isTrue() const;
    inline bool isInstanceOf(Traced<Class*> cls) const;

  private:
    enum Kind
    {
        ObjectKind = 0,
        IntKind = 1,

        PayloadShift = 1,
        KindMask = (1 << PayloadShift) - 1
    };

    union
    {
        Object* objectp;
        int32_t bits;
    };

    int32_t kind() const { return bits & KindMask; }
    int32_t payload() const { return bits >> PayloadShift; }
};

ostream& operator<<(ostream& s, const Value& v);

template <typename W>
struct WrapperMixins<W, Value>
{
    inline bool isObject() const;
    inline Object *asObject() const;
    inline Object *toObject() const;
    inline bool isInt32() const;
    inline int32_t asInt32() const;
    inline Value getAttr(Name name) const;
    inline bool maybeGetAttr(Name name, Root<Value>& valueOut) const;
    inline Class* type() const;
    inline bool isTrue() const;
    inline bool isInstanceOf(Traced<Class*> cls) const;

  private:
    const Value* get() const {
        return static_cast<const W*>(this)->location();
    }
};

template <>
struct GCTraits<Value>
{
    static Value nullValue();
    static bool isNonNull(Value value);
    static void checkValid(Value value);
    static void trace(Tracer& t, Value* v);
};

template <>
struct std::hash<Value> {
    size_t operator()(Value v) const {
        return v.hash();
    }
};

template <typename W, typename T>
struct WrapperMixins<W, T*>
{
    operator Traced<Value> () const {
        static_assert(is_base_of<Object, T>::value,
                      "T must derive from object for this conversion");
        // Since Traced<T> is immutable and all Objects are Values, we can
        // safely cast a Root<Object*> to a Traced<Value>.
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

#endif
