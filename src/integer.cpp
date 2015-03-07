#include "integer.h"

#include "bool.h"
#include "callable.h"
#include "singletons.h"
#include "string.h"
#include "value-inl.h"

#include <cmath>
#include <sstream>

struct UnaryPlus        { static int op(int a) { return a; } };
struct UnaryMinus       { static int op(int a) { return -a; } };
struct UnaryInvert      { static int op(int a) { return ~a; } };
struct BinaryPlus       { static int op(int a, int b) { return a + b; } };
struct BinaryMinus      { static int op(int a, int b) { return a - b; } };
struct BinaryMultiply   { static int op(int a, int b) { return a * b; } };
struct BinaryDivide     { static int op(int a, int b) { return a / b; } };
struct BinaryIntDivide  { static int op(int a, int b) { return a / b; } };
struct BinaryModulo     { static int op(int a, int b) { return a % b; } };
struct BinaryPower      { static int op(int a, int b) { return pow(a, b); } };
struct BinaryOr         { static int op(int a, int b) { return a | b; } };
struct BinaryXor        { static int op(int a, int b) { return a ^ b; } };
struct BinaryAnd        { static int op(int a, int b) { return a & b; } };
struct BinaryLeftShift  { static int op(int a, int b) { return a << b; } };
struct BinaryRightShift { static int op(int a, int b) { return a >> b; } };
struct CompareLT        { static int op(int a, int b) { return a < b; } };
struct CompareLE        { static int op(int a, int b) { return a <= b; } };
struct CompareGT        { static int op(int a, int b) { return a > b; } };
struct CompareGE        { static int op(int a, int b) { return a >= b; } };
struct CompareEQ        { static int op(int a, int b) { return a == b; } };
struct CompareNE        { static int op(int a, int b) { return a != b; } };

template <typename T>
static bool intUnaryOp(Traced<Value> arg, Root<Value>& resultOut)
{
    if (arg.isInt32())
        resultOut = Integer::get(T::op(arg.asInt32()));
    else
        resultOut = NotImplemented;
    return true;
}

template <typename T>
static bool intBinaryOp(Traced<Value> arg1, Traced<Value> arg2,
                        Root<Value>& resultOut)
{
    if (arg1.isInt32() && arg2.isInt32())
        resultOut = Integer::get(T::op(arg1.asInt32(), arg2.asInt32()));
    else
        resultOut = NotImplemented;
    return true;
}

template <typename T>
static bool intCompareOp(Traced<Value> arg1, Traced<Value> arg2,
                         Root<Value>& resultOut)
{
    if (arg1.isInt32() && arg2.isInt32())
        resultOut = Boolean::get(T::op(arg1.asInt32(), arg2.asInt32()));
    else
        resultOut = NotImplemented;
    return true;
}

static bool intStr(Traced<Value> arg, Root<Value>& resultOut) {
    if (!arg.isInt32()) {
        resultOut = NotImplemented;
        return true;
    }

    ostringstream s;
    s << dec << arg.asInt32();
    resultOut = String::get(s.str());
    return true;
}

GlobalRoot<Class*> Integer::ObjectClass;
GlobalRoot<Integer*> Integer::Zero;

template <typename T>
static void initNativeMethod(Traced<Class*> cls, Name name, typename T::Func f)
{
    static_assert(is_base_of<Native, T>(), "Type parameter must derive Native");
    Root<Value> value;
    value = gc::create<T>(f);
    cls->setAttr(name, value);
}

void Integer::init()
{
    Root<Class*> cls(gc::create<Class>("int"));
    Root<Value> value;
    initNativeMethod<Native1>(cls, "__str__", intStr);
    initNativeMethod<Native1>(cls, "__pos__", intUnaryOp<UnaryPlus>);
    initNativeMethod<Native1>(cls, "__neg__", intUnaryOp<UnaryMinus>);
    initNativeMethod<Native1>(cls, "__invert__", intUnaryOp<UnaryInvert>);
    initNativeMethod<Native2>(cls, "__lt__", intCompareOp<CompareLT>);
    initNativeMethod<Native2>(cls, "__le__", intCompareOp<CompareLE>);
    initNativeMethod<Native2>(cls, "__gt__", intCompareOp<CompareGT>);
    initNativeMethod<Native2>(cls, "__ge__", intCompareOp<CompareGE>);
    initNativeMethod<Native2>(cls, "__eq__", intCompareOp<CompareEQ>);
    initNativeMethod<Native2>(cls, "__ne__", intCompareOp<CompareNE>);
    initNativeMethod<Native2>(cls, "__or__", intBinaryOp<BinaryOr>);
    initNativeMethod<Native2>(cls, "__xor__", intBinaryOp<BinaryXor>);
    initNativeMethod<Native2>(cls, "__and__", intBinaryOp<BinaryAnd>);
    initNativeMethod<Native2>(cls, "__lshift__", intBinaryOp<BinaryLeftShift>);
    initNativeMethod<Native2>(cls, "__rshift__", intBinaryOp<BinaryRightShift>);
    initNativeMethod<Native2>(cls, "__add__", intBinaryOp<BinaryPlus>);
    initNativeMethod<Native2>(cls, "__sub__", intBinaryOp<BinaryMinus>);
    initNativeMethod<Native2>(cls, "__mul__", intBinaryOp<BinaryMultiply>);
    initNativeMethod<Native2>(cls, "__div__", intBinaryOp<BinaryDivide>);
    initNativeMethod<Native2>(cls, "__floordiv__", intBinaryOp<BinaryIntDivide>);
    initNativeMethod<Native2>(cls, "__mod__", intBinaryOp<BinaryModulo>);
    initNativeMethod<Native2>(cls, "__pow__", intBinaryOp<BinaryPower>);
    ObjectClass.init(cls);

    Zero.init(gc::create<Integer>(0));
}

Integer::Integer(int v)
  : Object(ObjectClass), value_(v)
{}

Value Integer::get(int v)
{
    if (v == 0)
        return Value(Zero);
    else if (v >= INT16_MIN && v <= INT16_MAX)
        return Value(v);
    else
        return getObject(v);
}

Object* Integer::getObject(int v)
{
    return gc::create<Integer>(v);
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
