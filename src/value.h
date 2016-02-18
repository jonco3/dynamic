#ifndef __VALUE_H__
#define __VALUE_H__

#include "gcdefs.h"
#include "name.h"

#include <gmpxx.h>

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
    Value(double value) { setDouble(value); }
    Value(const Value& other) : bits(other.bits) {}

    Value& operator=(Object* const & obj) {
        setObject(obj);
        return *this;
    }

    Value& operator=(int32_t value) {
        setInt32(value);
        return *this;
    }

    Value& operator=(double value) {
        setDouble(value);
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

    void setDouble(double value);

    bool isObject() const {
        return tagged.tag == ObjectTag;
    }

    bool isInt32() const {
        return tagged.tag == IntTag;
    }

    bool isDouble() const {
        return tagged.tag > IntTag;
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

    double asDouble() const {
        assert(isDouble());
        DoublePun p;
        p.bits = bits ^ DoubleXorConstant;
        return p.value;
    }

    inline Class* type() const;
    inline bool isInstanceOf(Traced<Class*> cls) const;

    template <typename T> bool isInstanceOf() const {
        return isInstanceOf(T::ObjectClass);
    }

    template <typename T> inline bool is() const;
    template <typename T> inline T* as() const;

    inline bool isNone() const;
    inline bool isInt() const;
    inline bool isFloat() const;

    inline Object *toObject() const;
    bool toInt32(int32_t& out) const;
    bool toSize(size_t& out) const;
    inline double toFloat() const;

    inline Value getAttr(Name name) const;
    inline bool maybeGetAttr(Name name, MutableTraced<Value> valueOut) const;

    // Not python equality!
    bool operator==(const Value& other) const { return bits == other.bits; }
    bool operator!=(const Value& other) const { return !(*this == other); }

    static bool IsTrue(Traced<Value> value);

  private:
    enum
    {
        ObjectTag = 0,
        IntTag = 1,
    };

    static const size_t PayloadBits = 48;
    static const size_t TagBits = 16;
    static const uint64_t TagMask = UINT64_C(0xffff000000000000);

    static const uint64_t DoubleXorConstant  = UINT64_C(0x7fff000000000000);
    static const uint64_t DoubleExponentMask = UINT64_C(0x7ff0000000000000);
    static const uint64_t DoubleMantissaMask = UINT64_C(0x000fffffffffffff);

    union DoublePun
    {
        double value;
        uint64_t bits;
    };

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

    inline Object* objectOrType() const;
    inline bool isNaNOrInfinity(uint64_t bits);
    uint64_t canonicalizeNaN(uint64_t bits);
};

ostream& operator<<(ostream& s, const Value& v);

#endif
