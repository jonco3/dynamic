#ifndef __VALUE_H__
#define __VALUE_H__

#include <ostream>

using namespace std;

struct Object;

struct Value
{
    Value() : objectp(nullptr) {}
    Value(Object* o) : objectp(o) {}

    bool isObject() const { return true; }
    Object *asObject() const { return objectp; }
    Object *toObject() const { return objectp; }

    bool operator==(const Value& other) const { return objectp == other.objectp; }
    bool operator!=(const Value& other) const { return !(*this == other); }

  private:
    union
    {
        Object* objectp;
    };
};

ostream& operator<<(ostream& s, const Value& v);

#endif
