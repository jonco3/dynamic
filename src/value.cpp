#include "value-inl.h"

#include "object.h"

ostream& operator<<(ostream& s, const Value& v) {
    Object* obj = v.toObject();
    if (obj)
        s << *obj;
    else
        s << "(nullptr)";
    return s;
}

void GCTraits<Value>::checkValid(Value value)
{
    if (value.isObject()) {
        Object* o = value.asObject();
        if (o)
            o->checkValid();
    }
}
