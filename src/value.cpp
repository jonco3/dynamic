#include "value.h"
#include "object.h"

ostream& operator<<(ostream& s, const Value& v) {
    s << v.asObject();
    return s;
}

void GCTraits<Value>::checkValid(Value value)
{
    Object* o = value.asObject();
    if (o)
        o->checkValid();
}
