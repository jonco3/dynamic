#include "string.h"

#include "bool.h"
#include "callable.h"
#include "value-inl.h"
#include "singletons.h"

#include <iostream>

static bool str_add(Traced<Value> arg1, Traced<Value> arg2, Root<Value>& resultOut) {
    const string& a = arg1.toObject()->as<String>()->value();
    const string& b = arg2.toObject()->as<String>()->value();
    resultOut = String::get(a + b);
    return true;
}

static bool str_str(Traced<Value> arg, Root<Value>& resultOut) {
    resultOut = arg;
    return true;
}

static bool str_print(Traced<Value> arg, Root<Value>& resultOut) {
    const string& a = arg.toObject()->as<String>()->value();
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
    value = gc::create<Native2>(str_add);    cls->setAttr("__add__", value);
    value = gc::create<Native1>(str_str);    cls->setAttr("__str__", value);
    value = gc::create<Native1>(str_print);  cls->setAttr("_print", value);
    ObjectClass.init(cls);
    EmptyString.init(gc::create<String>(""));
}

String::String(const string& v)
  : Object(ObjectClass), value_(v)
{}

Value String::get(const string& v)
{
    if (v == "")
        return EmptyString;
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
