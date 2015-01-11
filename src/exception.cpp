#include "exception.h"

#include "callable.h"
#include "singletons.h"
#include "string.h"

GlobalRoot<Class*> Exception::ObjectClass;
GlobalRoot<Class*> StopIteration::ObjectClass;

static bool exception_init(Traced<Value> arg1, Traced<Value> arg2,
                           Root<Value>& resultOut)
{
    // todo: methods should check the type of self
    Exception* e = arg1.asObject()->as<Exception>();
    resultOut = None;
    Root<Value> className(String::get("Exception"));
    e->__init__(className, arg2);
    return true;
}

void Exception::init()
{
    Root<Class*> cls(gc::create<Class>("Exception"));  // todo: whatever this is really called
    Root<Value> value;
    value = gc::create<Native2>(exception_init); cls->setAttr("__init__", value);
    ObjectClass.init(cls);

    StopIteration::init();
}

void StopIteration::init()
{
    Root<Class*> cls(gc::create<Class>("StopIterator"));
    // todo: make this derive Exception
    ObjectClass.init(cls);
}

Exception::Exception(Traced<Class*> cls, const string& className, const string& message)
  : Object(cls)
{
    Root<Value> classNameValue, messageValue;
    classNameValue = String::get(className);
    messageValue = String::get(message);
    __init__(classNameValue, messageValue);
}

Exception::Exception(const string& className, const string& message)
  : Object(ObjectClass)
{
    Root<Value> classNameValue, messageValue;
    classNameValue = String::get(className);
    messageValue = String::get(message);
    __init__(classNameValue, messageValue);
}

void Exception::__init__(Traced<Value> className, Traced<Value> message)
{
    assert(className.asObject()->is<String>()); // todo: check
    assert(message.asObject()->is<String>()); // todo: check
    setAttr("className", className);
    setAttr("message", message);
}

string Exception::className() const
{
    Root<Value> value(getAttr("className"));
    return value.get().toObject()->as<String>()->value();
}

string Exception::message() const
{
    Root<Value> value(getAttr("message"));
    return value.get().toObject()->as<String>()->value();
}

string Exception::fullMessage() const
{
    string message = this->message();
    if (message == "")
        return className();
    else
        return className() + ": " + message;
}

void Exception::print(ostream& os) const
{
    os << fullMessage();
}

#ifdef BUILD_TESTS

#include "test.h"

testcase(exception)
{
    Root<Exception*> exc;
    exc = gc::create<Exception>("foo", "bar");
    testEqual(exc->className(), "foo");
    testEqual(exc->message(), "bar");
    testEqual(exc->fullMessage(), "foo: bar");
}

#endif
