#include "integer.h"

#include "bool.h"
#include "callable.h"
#include "value-inl.h"

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

    IntegerClass() : Class("int") {}

    void initNatives() {
        Root<Value> value;
        value = new Native1(int_pos);    setProp("__pos__", value);
        value = new Native1(int_neg);    setProp("__neg__", value);
        value = new Native1(int_invert); setProp("__invert__", value);
        value = new Native2(int_le);     setProp("__lt__", value);
        value = new Native2(int_le);     setProp("__le__", value);
        value = new Native2(int_gt);     setProp("__gt__", value);
        value = new Native2(int_ge);     setProp("__ge__", value);
        value = new Native2(int_eq);     setProp("__eq__", value);
        value = new Native2(int_ne);     setProp("__ne__", value);
        value = new Native2(int_or);     setProp("__or__", value);
        value = new Native2(int_xor);    setProp("__xor__", value);
        value = new Native2(int_and);    setProp("__and__", value);
        value = new Native2(int_lshift); setProp("__lshift__", value);
        value = new Native2(int_rshift); setProp("__rshift__", value);
        value = new Native2(int_add);    setProp("__add__", value);
        value = new Native2(int_sub);    setProp("__sub__", value);
        value = new Native2(int_mul);    setProp("__mul__", value);
        value = new Native2(int_div);    setProp("__div__", value);
        value = new Native2(int_floordiv); setProp("__floordiv__", value);
        value = new Native2(int_mod);    setProp("__mod__", value);
        value = new Native2(int_pow);    setProp("__pow__", value);
    }
};

GlobalRoot<Class*> Integer::ObjectClass;
GlobalRoot<Integer*> Integer::Zero;

void Integer::init()
{
    Root<IntegerClass*> cls(new IntegerClass);
    cls->initNatives();
    ObjectClass.init(cls);
    Zero.init(new Integer(0));
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
