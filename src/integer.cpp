#include "integer.h"

#include "bool.h"
#include "callable.h"

#include <cmath>
#include <sstream>

struct IntegerClass : public Class
{
#define define_unary_int_operator(name, op)                                   \
    static Value name(Value arg) {                                            \
        int a = arg.toObject()->as<Integer>()->value();                       \
        return Integer::get(op a);                                            \
    }

    define_unary_int_operator(int_pos, +);
    define_unary_int_operator(int_neg, -);
    define_unary_int_operator(int_invert, ~);

#undef define_unary_int_operator

#define define_binary_int_operator(name, op)                                  \
    static Value name(Value arg1, Value arg2) {                               \
        int a = arg1.toObject()->as<Integer>()->value();                      \
        int b = arg2.toObject()->as<Integer>()->value();                      \
        return Integer::get(a op b);                                          \
    }

    define_binary_int_operator(int_or, |);
    define_binary_int_operator(int_xor, ^);
    define_binary_int_operator(int_and, &);
    define_binary_int_operator(int_lshift, <<);
    define_binary_int_operator(int_rshift, >>);
    define_binary_int_operator(int_add, +);
    define_binary_int_operator(int_sub, -);
    define_binary_int_operator(int_mul, *);
    define_binary_int_operator(int_div, /);
    define_binary_int_operator(int_floordiv, /);
    define_binary_int_operator(int_mod, %);

#undef define_binary_int_operator

#define define_binary_bool_operator(name, op)                                 \
    static Value name(Value arg1, Value arg2) {                               \
        int a = arg1.toObject()->as<Integer>()->value();                      \
        int b = arg2.toObject()->as<Integer>()->value();                      \
        return Boolean::get(a op b);                                          \
    }

    define_binary_bool_operator(int_lt, <);
    define_binary_bool_operator(int_le, <=);
    define_binary_bool_operator(int_gt, >);
    define_binary_bool_operator(int_ge, >=);
    define_binary_bool_operator(int_eq, ==);
    define_binary_bool_operator(int_ne, !=);

#undef define_binary_bool_operator

    static Value int_pow(Value arg1, Value arg2) {
        int a = arg1.toObject()->as<Integer>()->value();
        int b = arg2.toObject()->as<Integer>()->value();
        return Integer::get(std::pow(a, b));
    }

    IntegerClass() : Class("int") {
        Root<Object> root(this);
        setProp("__pos__",      new Native1(int_pos));
        setProp("__neg__",      new Native1(int_neg));
        setProp("__invert__",   new Native1(int_invert));
        setProp("__lt__",       new Native2(int_le));
        setProp("__le__",       new Native2(int_le));
        setProp("__gt__",       new Native2(int_gt));
        setProp("__ge__",       new Native2(int_ge));
        setProp("__eq__",       new Native2(int_eq));
        setProp("__ne__",       new Native2(int_ne));
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

Root<Class> Integer::ObjectClass;
Root<Integer> Integer::Zero;

void Integer::init()
{
    ObjectClass = new IntegerClass;
    Zero = new Integer(0);
}

Integer::Integer(int v)
  : Object(ObjectClass), value_(v)
{}

Value Integer::get(int v)
{
    if (v == 0)
        return Zero;
    return new Integer(v);
}

void Integer::print(ostream& s) const {
    s << value_;
}
