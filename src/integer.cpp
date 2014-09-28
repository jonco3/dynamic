#include "integer.h"
#include "callable.h"

#include <cmath>
#include <sstream>

struct IntegerClass : public Class
{
#define define_int_method(name, op)                                           \
    static Value name(Value arg1, Value arg2) {                               \
        int a = arg1.toObject()->as<Integer>()->value();                      \
        int b = arg2.toObject()->as<Integer>()->value();                      \
        return Integer::get(a op b);                                          \
    }

    define_int_method(lshift, <<);
    define_int_method(rshift, >>);
    define_int_method(add, +);
    define_int_method(sub, -);
    define_int_method(mul, *);
    define_int_method(div, /);
    define_int_method(floordiv, /);
    define_int_method(mod, %);

    static Value pow(Value arg1, Value arg2) {
        int a = arg1.toObject()->as<Integer>()->value();
        int b = arg2.toObject()->as<Integer>()->value();
        return Integer::get(std::pow(a, b));
    }

    IntegerClass() : Class("Integer") {
        setProp("__lshift__", new Native2(lshift));
        setProp("__rshift__", new Native2(rshift));
        setProp("__add__", new Native2(add));
        setProp("__sub__", new Native2(sub));
        setProp("__mul__", new Native2(mul));
        setProp("__div__", new Native2(div));
        setProp("__floordiv__", new Native2(floordiv));
        setProp("__mod__", new Native2(mod));
        setProp("__pow__", new Native2(pow));
    }
};

IntegerClass* Integer::ObjectClass;

void Integer::init()
{
    ObjectClass = new IntegerClass;
}

Integer::Integer(int v)
  : Object(ObjectClass), value_(v)
{}

Value Integer::get(int v)
{
    return new Integer(v);
}

void Integer::print(ostream& s) const {
    s << value_;
}
