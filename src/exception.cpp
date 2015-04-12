#include "exception.h"

#include "callable.h"
#include "singletons.h"
#include "string.h"

GlobalRoot<Class*> Exception::ObjectClass;
GlobalRoot<Class*> StopIteration::ObjectClass;

void Exception::init()
{
    ObjectClass.init(gc::create<NativeClass>("Exception",
                                             &Exception::create, 2));
    StopIteration::init();
}

void StopIteration::init()
{
    ObjectClass.init(
        gc::create<Class>("StopIteration", Exception::ObjectClass));
}

Exception::Exception(Traced<Class*> cls, const TokenPos& pos,
                     const string& message)
  : Object(cls), pos_(pos)
{
    Root<Value> classNameValue, messageValue;
    classNameValue = String::get(cls->name());
    messageValue = String::get(message);
    init(classNameValue, messageValue);
}

Exception::Exception(const string& className, const string& message)
  : Object(ObjectClass)
{
    Root<Value> classNameValue, messageValue;
    classNameValue = String::get(className);
    messageValue = String::get(message);
    init(classNameValue, messageValue);
}

bool Exception::create(TracedVector<Value> args, Root<Value>& resultOut)
{
    assert(args.size() == 2);
    if (!checkInstanceOf(args[1], String::ObjectClass, resultOut))
        return false;

    resultOut = gc::create<Exception>("Exception",
                                      args[1].toObject()->as<String>()->value());
    return true;
}

void Exception::init(Traced<Value> className, Traced<Value> message)
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

bool checkInstanceOf(Traced<Value> v, Traced<Class*> cls, Root<Value>& resultOut)
{
    if (!v.isInstanceOf(cls)) {
        string message = "Excpecting" + cls->name() +
            " but got " + v.type()->name();
        resultOut = gc::create<Exception>("TypeError", message);
        return false;
    }

    return true;
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
