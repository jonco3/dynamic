#include "integer.h"

#include "bool.h"
#include "callable.h"
#include "string.h"
#include "value-inl.h"

#include <cmath>
#include <sstream>

struct IntegerClass : public Class
{
    static bool int_str(Traced<Value> arg, Root<Value>& resultOut) {
        int a = arg.toObject()->as<Integer>()->value();
        ostringstream s;
        s << dec << a;
        resultOut = String::get(s.str());
        return true;
    }

#define define_unary_int_operator(name, op)                                   \
    static bool name(Traced<Value> arg, Root<Value>& resultOut) {             \
        int a = arg.toObject()->as<Integer>()->value();                       \
        resultOut = Integer::get(op a);                                       \
        return true;                                                          \
    }

    define_unary_int_operator(int_pos, +);
    define_unary_int_operator(int_neg, -);
    define_unary_int_operator(int_invert, ~);

#undef define_unary_int_operator

#define define_binary_int_operator(name, op)                                  \
    static bool name(Traced<Value> arg1, Traced<Value> arg2,                  \
                     Root<Value>& resultOut) {                                \
        int a = arg1.toObject()->as<Integer>()->value();                      \
        int b = arg2.toObject()->as<Integer>()->value();                      \
        resultOut = Integer::get(a op b);                                     \
        return true;                                                          \
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
    static bool name(Traced<Value> arg1, Traced<Value> arg2,                  \
                     Root<Value>& resultOut) {                                \
        int a = arg1.toObject()->as<Integer>()->value();                      \
        int b = arg2.toObject()->as<Integer>()->value();                      \
        resultOut = Boolean::get(a op b);                                     \
        return true;                                                          \
    }

    define_binary_bool_operator(int_lt, <);
    define_binary_bool_operator(int_le, <=);
    define_binary_bool_operator(int_gt, >);
    define_binary_bool_operator(int_ge, >=);
    define_binary_bool_operator(int_eq, ==);
    define_binary_bool_operator(int_ne, !=);

#undef define_binary_bool_operator

    static bool int_pow(Traced<Value> arg1, Traced<Value> arg2,
                        Root<Value>& resultOut) {
        int a = arg1.toObject()->as<Integer>()->value();
        int b = arg2.toObject()->as<Integer>()->value();
        resultOut = Integer::get(std::pow(a, b));
        return true;
    }

    IntegerClass() : Class("int") {}

    void initNatives() {
        Root<Value> value;
        value = new Native1(int_str);    setAttr("__str__", value);
        value = new Native1(int_pos);    setAttr("__pos__", value);
        value = new Native1(int_neg);    setAttr("__neg__", value);
        value = new Native1(int_invert); setAttr("__invert__", value);
        value = new Native2(int_le);     setAttr("__lt__", value);
        value = new Native2(int_le);     setAttr("__le__", value);
        value = new Native2(int_gt);     setAttr("__gt__", value);
        value = new Native2(int_ge);     setAttr("__ge__", value);
        value = new Native2(int_eq);     setAttr("__eq__", value);
        value = new Native2(int_ne);     setAttr("__ne__", value);
        value = new Native2(int_or);     setAttr("__or__", value);
        value = new Native2(int_xor);    setAttr("__xor__", value);
        value = new Native2(int_and);    setAttr("__and__", value);
        value = new Native2(int_lshift); setAttr("__lshift__", value);
        value = new Native2(int_rshift); setAttr("__rshift__", value);
        value = new Native2(int_add);    setAttr("__add__", value);
        value = new Native2(int_sub);    setAttr("__sub__", value);
        value = new Native2(int_mul);    setAttr("__mul__", value);
        value = new Native2(int_div);    setAttr("__div__", value);
        value = new Native2(int_floordiv); setAttr("__floordiv__", value);
        value = new Native2(int_mod);    setAttr("__mod__", value);
        value = new Native2(int_pow);    setAttr("__pow__", value);
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
    s << dec << value_;
}
