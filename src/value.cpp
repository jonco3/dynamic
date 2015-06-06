#include "value-inl.h"

#include "object.h"

ostream& operator<<(ostream& s, const Value& v) {
    if (v.isInt32())
        s << v.asInt32();
    else {
        Object* obj = v.toObject();
        if (obj)
            s << *obj;
        else
            s << "(nullptr)";
    }
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
