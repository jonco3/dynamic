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
    Root<Class*> cls(gc::create<Class>("str"));
    Root<Value> value;
    value = gc::create<Native>(2, str_add);    cls->setAttr("__add__", value);
    value = gc::create<Native>(1, str_str);    cls->setAttr("__str__", value);
    value = gc::create<Native>(1, str_print);  cls->setAttr("_print", value);
    ObjectClass.init(cls);
    EmptyString.init(gc::create<String>(""));
}

String::String(const string& v)
  : Object(ObjectClass), value_(v)
{}

Value String::get(const string& v)
{
    if (v == "")
        return Value(EmptyString);
    // todo: can intern short strings here
    return gc::create<String>(v);
}

void String::print(ostream& s) const {
    s << "'" << value_ << "'";
}

#ifdef BUILD_TESTS

#include "test.h"

testcase(string)
{
    testInterp("\"foo\"", "'foo'");
    testInterp("'foo'", "'foo'");
    testInterp("\"\"", "''");
    testInterp("''", "''");
    testInterp("\"foo\" + 'bar'", "'foobar'");
}

#endif
