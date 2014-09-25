#include "integer.h"
#include "callable.h"

#include <sstream>

struct IntegerClass : public Class
{
    static Value plus(Value a, Value b) {
        Object *ao = a.toObject();
        Object *bo = b.toObject();
        return Integer::get(ao->as<Integer>()->toInt() + bo->as<Integer>()->toInt());
    }

    IntegerClass() {
        setProp("__plus__", new Native2(plus));
    }
};

IntegerClass Integer::Class;

Integer::Integer(int v)
  : Object(&Class), value(v)
{}

Value Integer::get(int v)
{
    return new Integer(v);
}

void Integer::print(ostream& s) const {
    s << value;
}
