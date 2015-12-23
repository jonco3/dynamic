#include "string.h"

#include "callable.h"
#include "exception.h"
#include "interp.h"
#include "value-inl.h"
#include "singletons.h"
#include "slice.h"

#include <iostream>

typedef bool (StringCompareOp)(const string&, const string&);
static bool stringLT(const string& a, const string& b) { return a < b; }
static bool stringLE(const string& a, const string& b) { return a <= b; }
static bool stringGT(const string& a, const string& b) { return a > b; }
static bool stringGE(const string& a, const string& b) { return a >= b; }
static bool stringEQ(const string& a, const string& b) { return a == b; }
static bool stringNE(const string& a, const string& b) { return a != b; }

// todo: use this to replace str() function.
static bool valueToString(Traced<Value> value, MutableTraced<Value> resultOut)
{
    if (value.is<String>()) {
        resultOut = value;
        return true;
    }

    Stack<Value> strFunc;
    if (!value.maybeGetAttr(Names::__str__, strFunc) &&
        !value.maybeGetAttr(Names::__repr__, strFunc))
    {
        resultOut = gc.create<TypeError>(
            "Object has no __str__ or __repr__ method");
        return false;
    }

    interp->pushStack(value);
    if (!interp->call(strFunc, 1, resultOut))
        return false;

    if (!resultOut.is<String>()) {
        resultOut =
            gc.create<TypeError>("__str__ method should return a string");
        return false;
    }

    return true;
}

static bool str_new(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], Class::ObjectClass, resultOut))
        return false;

    if (args.size() == 1) {
        resultOut = String::get("");
        return true;
    }

    return valueToString(args[1], resultOut);
}

static bool str_add(TracedVector<Value> args, MutableTraced<Value> resultOut) {
    if (!args[0].isInstanceOf<String>() || !args[1].isInstanceOf<String>()) {
        resultOut = NotImplemented;
        return true;
    }

    resultOut = String::get(args[0].as<String>()->value() +
                            args[1].as<String>()->value());
    return true;
}

static bool str_str(TracedVector<Value> args, MutableTraced<Value> resultOut) {
    resultOut = args[0];
    return true;
}

static bool stringValue(Traced<Value> value, string& out)
{
    if (!value.is<String>())
        return false;

    out = value.as<const String>()->value();
    return true;
}

template <StringCompareOp op>
static bool stringCompareOp(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    string a;
    if (!stringValue(args[0], a)) {
        resultOut = NotImplemented;
        return true;
    }

    string b;
    if (!stringValue(args[1], b)) {
        resultOut = NotImplemented;
        return true;
    }

    resultOut = Boolean::get(op(a, b));
    return true;
}

static bool str_len(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    const string& s = args[0].as<String>()->value();
    resultOut = Integer::get(s.size());
    return true;
}

static bool str_getitem(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    Stack<String*> s(args[0].as<String>());
    return s->getitem(args[1], resultOut);
}

static bool str_hash(TracedVector<Value> args, MutableTraced<Value> resultOut) {
    const string& a = args[0].as<String>()->value();
    // todo: should probably use python hash algorithm for this.
    resultOut = Integer::get(std::hash<string>()(a));
    return true;
}

static bool str_contains(TracedVector<Value> args, MutableTraced<Value> resultOut) {
    const string& a = args[0].as<String>()->value();
    const string& b = args[1].as<String>()->value();
    resultOut = Boolean::get(a.find(b) != string::npos);
    return true;
}

static bool str_print(TracedVector<Value> args, MutableTraced<Value> resultOut) {
    const string& a = args[0].as<String>()->value();
    cout << a << endl;
    resultOut = None;
    return true;
}

GlobalRoot<Class*> String::ObjectClass;
GlobalRoot<String*> String::EmptyString;

void String::init()
{
    ObjectClass.init(Class::createNative("str", str_new, 2));

    initNativeMethod(ObjectClass, "__len__", str_len, 1);
    initNativeMethod(ObjectClass, "__getitem__", str_getitem, 2);
    initNativeMethod(ObjectClass, "__add__", str_add, 2);
    initNativeMethod(ObjectClass, "__str__", str_str, 1);
    initNativeMethod(ObjectClass, "__eq__", stringCompareOp<stringEQ>, 2);
    initNativeMethod(ObjectClass, "__ne__", stringCompareOp<stringNE>, 2);
    initNativeMethod(ObjectClass, "__lt__", stringCompareOp<stringLT>, 2);
    initNativeMethod(ObjectClass, "__le__", stringCompareOp<stringLE>, 2);
    initNativeMethod(ObjectClass, "__gt__", stringCompareOp<stringGT>, 2);
    initNativeMethod(ObjectClass, "__ge__", stringCompareOp<stringGE>, 2);
    initNativeMethod(ObjectClass, "__hash__", str_hash, 1);
    initNativeMethod(ObjectClass, "__contains__", str_contains, 2);
    initNativeMethod(ObjectClass, "_print", str_print, 1);

    EmptyString.init(gc.create<String>(""));
}

String::String(const string& v)
  : Object(ObjectClass), value_(v)
{}

String::String(Traced<Class*> cls)
  : Object(cls)
{
    assert(cls->isDerivedFrom(ObjectClass));
}

String* String::get(const string& v)
{
    if (v == "")
        return EmptyString;
    // todo: can intern short strings here
    return gc.create<String>(v);
}

void String::print(ostream& s) const {
    s << "'" << value_ << "'";
}

static bool raiseOutOfRange(MutableTraced<Value> resultOut)
{
    resultOut = gc.create<IndexError>("string index out of range");
    return false;
}

bool String::getitem(Traced<Value> index, MutableTraced<Value> resultOut)
{
    size_t len = value_.size();
    if (index.isInt()) {
        int32_t i;
        if (!index.toInt32(&i))
            return raiseOutOfRange(resultOut);
        i = WrapIndex(i, len);
        if (i < 0 || size_t(i) >= len)
            return raiseOutOfRange(resultOut);
        resultOut = get(string(1, value_[i]));
        return true;
    } else if (index.isInstanceOf(Slice::ObjectClass)) {
        Stack<Slice*> slice(index.as<Slice>());
        int32_t start, count, step;
        if (!slice->getIterationData(len, start, count, step, resultOut))
            return false;

        string result;
        int32_t src = start;
        for (size_t i = 0; i < count; i++) {
            assert(src < len);
            result += value_[src];
            src += step;
        }

        resultOut = String::get(result);
        return true;
    } else {
        resultOut = gc.create<TypeError>(
            "string indices must be integers or slices");
        return false;
    }
}
