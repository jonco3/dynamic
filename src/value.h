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
    Value() { setObject(nullptr); }
    Value(Object* obj) { setObject(obj); }
    Value(int32_t value) { setInt32(value); }
    Value(const Value& other) : bits(other.bits) {}

    Value& operator=(Object* const & obj) {
        setObject(obj);
        return *this;
    }

    Value& operator=(int32_t value) {
        setInt32(value);
        return *this;
    }

    void setObject(Object* obj) {
        objectp = obj;
        assert(isObject());
    }

    void setInt32(int32_t value) {
        tagged.payload = (uint64_t)value;
        tagged.tag = IntTag;
        assert(isInt32());
    }

    bool isObject() const {
        return tagged.tag == ObjectTag;
    }

    bool isInt32() const {
        return tagged.tag == IntTag;
    }

    Object* asObject() const {
        assert(isObject());
        return objectp;
    }

    Object* maybeObject() const {
        return isObject() ? asObject() : nullptr;
    }

    int32_t asInt32() const {
        assert(isInt32());
        return int32.value;
    }

    inline Class* type() const;
    inline bool isInstanceOf(Traced<Class*> cls) const;

    template <typename T> inline bool is() const;
    template <typename T> inline T* as();
    template <typename T> inline const T* as() const;

    inline bool isNone() const;
    inline bool isTrue() const;
    inline bool isInt() const;

    inline Object *toObject() const;
    inline int32_t toInt32() const;

    inline Value getAttr(Name name) const;
    inline bool maybeGetAttr(Name name, MutableTraced<Value> valueOut) const;

    // Not python equality!
    bool operator==(const Value& other) const { return bits == other.bits; }
    bool operator!=(const Value& other) const { return !(*this == other); }

  private:
    enum
    {
        ObjectTag = 0,
        IntTag = 1,
    };

    static const size_t PayloadBits = 48;
    static const size_t TagBits = 16;
    static const size_t TagMask = 0xffff000000000000;

    struct PayloadAndTag
    {
        uint64_t payload:PayloadBits;
        uint64_t tag:TagBits;
    };

    struct TaggedInt32
    {
        int32_t value;
        int32_t rest;
    };

    union
    {
        uintptr_t bits;
        PayloadAndTag tagged;
        Object* objectp;
        TaggedInt32 int32;
    };
};

ostream& operator<<(ostream& s, const Value& v);

#endif
