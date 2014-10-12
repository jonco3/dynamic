#include "value.h"
#include "object.h"

ostream& operator<<(ostream& s, const Value& v) {
    s << v.asObject();
    return s;
}

void gc::checkValid(Value v)
{
    v.asObject()->checkValid();
}

void gc::trace(Tracer& t, const Value* v)
{
    Object* o = v->asObject();
    gc::trace(t, &o);
    *const_cast<Value*>(v) = Value(o);
}
