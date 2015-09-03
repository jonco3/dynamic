#include "string.h"

#include "bool.h"
#include "callable.h"
#include "value-inl.h"
#include "singletons.h"

#include <iostream>

typedef bool (StringCompareOp)(const string&, const string&);
static bool stringLT(const string& a, const string& b) { return a < b; }
static bool stringLE(const string& a, const string& b) { return a <= b; }
static bool stringGT(const string& a, const string& b) { return a > b; }
static bool stringGE(const string& a, const string& b) { return a >= b; }
static bool stringEQ(const string& a, const string& b) { return a == b; }
static bool stringNE(const string& a, const string& b) { return a != b; }

static bool str_add(TracedVector<Value> args, MutableTraced<Value> resultOut) {
    const string& a = args[0].as<String>()->value();
    const string& b = args[1].as<String>()->value();
    resultOut = String::get(a + b);
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
    ObjectClass.init(Class::createNative<String>("str"));

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
