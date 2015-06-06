#include "string.h"

#include "bool.h"
#include "callable.h"
#include "value-inl.h"
#include "singletons.h"

#include <iostream>

static bool str_add(TracedVector<Value> args, Root<Value>& resultOut) {
    const string& a = args[0].toObject()->as<String>()->value();
    const string& b = args[1].toObject()->as<String>()->value();
    resultOut = String::get(a + b);
    return true;
}

static bool str_str(TracedVector<Value> args, Root<Value>& resultOut) {
    resultOut = args[0];
    return true;
}

static bool str_eq(TracedVector<Value> args, Root<Value>& resultOut) {
    const string& a = args[0].toObject()->as<String>()->value();
    const string& b = args[1].toObject()->as<String>()->value();
    resultOut = Boolean::get(a == b);
    return true;
}

static bool str_hash(TracedVector<Value> args, Root<Value>& resultOut) {
    const string& a = args[0].toObject()->as<String>()->value();
    // todo: should probably use python hash algorithm for this.
    resultOut = Integer::get(std::hash<string>()(a));
    return true;
}

static bool str_contains(TracedVector<Value> args, Root<Value>& resultOut) {
    const string& a = args[0].toObject()->as<String>()->value();
    const string& b = args[1].toObject()->as<String>()->value();
    resultOut = Boolean::get(a.find(b) != string::npos);
    return true;
}

static bool str_print(TracedVector<Value> args, Root<Value>& resultOut) {
    const string& a = args[0].toObject()->as<String>()->value();
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
    initNativeMethod(ObjectClass, "__eq__", str_eq, 2);
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

Value String::get(const string& v)
{
    if (v == "")
        return Value(EmptyString);
    // todo: can intern short strings here
    return gc.create<String>(v);
}

void String::print(ostream& s) const {
    s << "'" << value_ << "'";
}
