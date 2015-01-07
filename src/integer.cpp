#include "integer.h"

#include "bool.h"
#include "callable.h"
#include "string.h"
#include "value-inl.h"

#include <cmath>
#include <sstream>

static bool int_str(Traced<Value> arg, Root<Value>& resultOut) {
    int a = arg.asInt32();
    ostringstream s;
    s << dec << a;
    resultOut = String::get(s.str());
    return true;
}

#define define_unary_int_operator(name, op)                                   \
    static bool name(Traced<Value> arg, Root<Value>& resultOut) {             \
        int a = arg.asInt32();                                                \
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
        int a = arg1.asInt32();                                               \
        int b = arg2.asInt32();                                               \
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
        int a = arg1.asInt32();                                               \
        int b = arg2.asInt32();                                               \
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
    int a = arg1.asInt32();
    int b = arg2.asInt32();
    resultOut = Integer::get(std::pow(a, b));
    return true;
}

GlobalRoot<Class*> Integer::ObjectClass;
GlobalRoot<Integer*> Integer::Zero;
GlobalRoot<Integer*> Integer::Proto;

void Integer::init()
{
    Root<Class*> cls(new Class("int"));
    Root<Value> value;
    value = new Native1(int_str);    cls->setAttr("__str__", value);
    value = new Native1(int_pos);    cls->setAttr("__pos__", value);
    value = new Native1(int_neg);    cls->setAttr("__neg__", value);
    value = new Native1(int_invert); cls->setAttr("__invert__", value);
    value = new Native2(int_lt);     cls->setAttr("__lt__", value);
    value = new Native2(int_le);     cls->setAttr("__le__", value);
    value = new Native2(int_gt);     cls->setAttr("__gt__", value);
    value = new Native2(int_ge);     cls->setAttr("__ge__", value);
    value = new Native2(int_eq);     cls->setAttr("__eq__", value);
    value = new Native2(int_ne);     cls->setAttr("__ne__", value);
    value = new Native2(int_or);     cls->setAttr("__or__", value);
    value = new Native2(int_xor);    cls->setAttr("__xor__", value);
    value = new Native2(int_and);    cls->setAttr("__and__", value);
    value = new Native2(int_lshift); cls->setAttr("__lshift__", value);
    value = new Native2(int_rshift); cls->setAttr("__rshift__", value);
    value = new Native2(int_add);    cls->setAttr("__add__", value);
    value = new Native2(int_sub);    cls->setAttr("__sub__", value);
    value = new Native2(int_mul);    cls->setAttr("__mul__", value);
    value = new Native2(int_div);    cls->setAttr("__div__", value);
    value = new Native2(int_floordiv); cls->setAttr("__floordiv__", value);
    value = new Native2(int_mod);    cls->setAttr("__mod__", value);
    value = new Native2(int_pow);    cls->setAttr("__pow__", value);
    ObjectClass.init(cls);

    Zero.init(new Integer(0));
    Proto.init(new Integer(INT16_MAX + 1));
}

Integer::Integer(int v)
  : Object(ObjectClass), value_(v)
{}

Value Integer::get(int v)
{
    if (v == 0)
        return Zero;
    else if (v >= INT16_MIN && v <= INT16_MAX)
        return Value(v);
    else
        return getObject(v);
}

Object* Integer::getObject(int v)
{
    return new Integer(v);
}

void Integer::print(ostream& s) const {
    s << dec << value_;
}

#ifdef BUILD_TESTS

#include "test.h"

testcase(integer)
{
    testFalse(Integer::get(1).isObject());
    testFalse(Integer::get(32767).isObject());
    testTrue(Integer::get(32768).isObject());

    testInterp("1", "1");
    testInterp("2 + 2", "4");
    testInterp("5 - 2", "3");
    testInterp("1 + 0", "1");
    testInterp("+3", "3");
    testInterp("-4", "-4");
    testInterp("32767 + 1", "32768");
    testInterp("32768 - 1", "32767");
}

#endif
