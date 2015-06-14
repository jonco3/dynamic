#ifndef __VALUE_H__
#define __VALUE_H__

#include "gcdefs.h"
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

    inline bool isInt32() const;
    inline int32_t asInt32() const;

    bool isObject() const {
        return kind() == ObjectKind;
    }

    Object* asObject() const {
        assert(isObject());
        return objectp;
    }

    Object* maybeObject() const {
        return isObject() ? asObject() : nullptr;
    }

    inline Object *toObject() const;

    inline bool isNone() const;

    inline Class* type() const;

    template <typename T> inline bool is() const;
    template <typename T> inline T* as();

    inline Value getAttr(Name name) const;
    inline bool maybeGetAttr(Name name, Root<Value>& valueOut) const;

    bool operator==(const Value& other) const { return bits == other.bits; }
    bool operator!=(const Value& other) const { return !(*this == other); }

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

#endif
