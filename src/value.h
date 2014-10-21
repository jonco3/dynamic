#ifndef __VALUE_H__
#define __VALUE_H__

#include "gc.h"

#include <ostream>

using namespace std;

struct Object;
struct Tracer;

struct Value
{
    Value() : objectp(nullptr) {}
    Value(Object* o) : objectp(o) {}

    // todo
    template <typename T>
    Value(Root<T>& o) : objectp(o.get()) {}
    template <typename T>
    Value(GlobalRoot<T>& o) : objectp(o.get()) {}

    bool isObject() const { return true; }
    Object *asObject() const { return objectp; }
    Object *toObject() const { return objectp; }

    bool operator==(const Value& other) const { return objectp == other.objectp; }
    bool operator!=(const Value& other) const { return !(*this == other); }

    inline bool isTrue() const;

  private:
    union
    {
        Object* objectp;
    };
};

ostream& operator<<(ostream& s, const Value& v);

#endif
