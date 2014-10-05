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

    define_int_method(int_or, |);
    define_int_method(int_xor, ^);
    define_int_method(int_and, &);
    define_int_method(int_lshift, <<);
    define_int_method(int_rshift, >>);
    define_int_method(int_add, +);
    define_int_method(int_sub, -);
    define_int_method(int_mul, *);
    define_int_method(int_div, /);
    define_int_method(int_floordiv, /);
    define_int_method(int_mod, %);

    static Value int_pow(Value arg1, Value arg2) {
        int a = arg1.toObject()->as<Integer>()->value();
        int b = arg2.toObject()->as<Integer>()->value();
        return Integer::get(std::pow(a, b));
    }

    IntegerClass() : Class("Integer") {
        setProp("__or__",       new Native2(int_or));
        setProp("__xor__",      new Native2(int_xor));
        setProp("__and__",      new Native2(int_and));
        setProp("__lshift__",   new Native2(int_lshift));
        setProp("__rshift__",   new Native2(int_rshift));
        setProp("__add__",      new Native2(int_add));
        setProp("__sub__",      new Native2(int_sub));
        setProp("__mul__",      new Native2(int_mul));
        setProp("__div__",      new Native2(int_div));
        setProp("__floordiv__", new Native2(int_floordiv));
        setProp("__mod__",      new Native2(int_mod));
        setProp("__pow__",      new Native2(int_pow));
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
