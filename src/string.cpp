#include "string.h"

#include "bool.h"
#include "callable.h"
#include "value-inl.h"
#include "singletons.h"

#include <iostream>

struct StringClass : public Class
{
    StringClass() : Class("str") {}

    static bool str_add(Value arg1, Value arg2, Value& resultOut) {
        const string& a = arg1.toObject()->as<String>()->value();
        const string& b = arg2.toObject()->as<String>()->value();
        resultOut = String::get(a + b);
        return true;
    }

    static bool str_str(Value arg, Value& resultOut) {
        resultOut = arg;
        return true;
    }

    static bool str_print(Value arg, Value& resultOut) {
        const string& a = arg.toObject()->as<String>()->value();
        cout << a << endl;
        resultOut = None;
        return true;
    }

    void initNatives() {
        Root<Value> value;
        value = new Native2(str_add);    setAttr("__add__", value);
        value = new Native1(str_str);    setAttr("__str__", value);
        value = new Native1(str_print);  setAttr("_print", value);
    }
};

GlobalRoot<Class*> String::ObjectClass;
GlobalRoot<String*> String::EmptyString;

void String::init()
{
    Root<StringClass*> cls(new StringClass);
    cls->initNatives();
    ObjectClass.init(cls);
    EmptyString.init(new String(""));
}

String::String(const string& v)
  : Object(ObjectClass), value_(v)
{}

Value String::get(const string& v)
{
    if (v == "")
        return EmptyString;
    // todo: can intern short strings here
    return new String(v);
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
