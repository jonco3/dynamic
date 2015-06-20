#include "value-inl.h"

#include "object.h"

RootArray<Value, 0> EmptyValueArray;

ostream& operator<<(ostream& s, const Value& v) {
    if (v.isInt())
        s << v.toInt();
    else {
        Object* obj = v.toObject();
        if (obj)
            s << *obj;
        else
            s << "(nullptr)";
    }
    return s;
}
