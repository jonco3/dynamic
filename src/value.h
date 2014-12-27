#ifndef __VALUE_H__
#define __VALUE_H__

#include "gc.h"
#include "name.h"

#include <ostream>

using namespace std;

struct Integer;
struct Object;
struct Tracer;

struct Value
{
    Value() : objectp(nullptr) {}
    Value(Object* o) : objectp(o) {}
    inline Value(int16_t i);

    // todo
    template <typename T>
    Value(Root<T>& o) : objectp(o.get()) {}
    template <typename T>
    Value(GlobalRoot<T>& o) : objectp(o.get()) {}

    bool isObject() const {
        return type() == ObjectType;
    }

    Object *asObject() const {
        assert(isObject());
        return objectp;
    }

    inline Object *toObject() const;
    inline bool isInt32() const;
    inline int32_t asInt32() const;

    inline bool maybeGetAttr(Name name, Value& valueOut);

    bool operator==(const Value& other) const;
    bool operator!=(const Value& other) const { return !(*this == other); }

    size_t hash() const;

    inline bool isTrue() const;

  private:
    enum Type
    {
        ObjectType = 0,
        IntType = 1,

        PayloadShift = 1,
        TypeMask = (1 << PayloadShift) - 1
    };

    union
    {
        Object* objectp;
        int32_t bits;
    };

    int32_t type() const { return bits & TypeMask; }
    int32_t payload() const { return bits >> PayloadShift; }
};

ostream& operator<<(ostream& s, const Value& v);

template <>
struct TracedMixins<Value>
{
    inline bool isObject() const;
    inline Object *asObject() const;
    inline Object *toObject() const;
    inline bool isInt32() const;
    inline int32_t asInt32() const;

  private:
    const Value* get() const {
        return static_cast<const Traced<Value>*>(this)->location();
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


#endif
