#include "exception.h"

#include "callable.h"
#include "singletons.h"
#include "string.h"

#include <sstream>

GlobalRoot<Class*> Exception::ObjectClass;

#define define_exception_class(name)                                          \
    GlobalRoot<Class*> name::ObjectClass;
for_each_exception_class(define_exception_class)
#undef define_exception_class

void Exception::init()
{
    ObjectClass.init(gc.create<NativeClass>("Exception",
                                             None,
                                             &Exception::create, 2));

#define init_exception_class(name)                                          \
    name::ObjectClass.init(                                                 \
        gc.create<NativeClass>(#name,                                       \
                               Exception::ObjectClass,                      \
                               &Exception::create, 2));
    for_each_exception_class(init_exception_class)
#undef init_exception_class
}

Exception::Exception(Traced<Class*> cls, const string& message)
  : Object(cls)
{
    assert(cls->is<Class>());
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
    assert(args.size() >= 1 && args.size() <= 2);
    if (!checkInstanceOf(args[0], Class::ObjectClass, resultOut))
        return false;
    // todo: check class has exception as a base
    Root<Class*> cls(args[0].toObject()->as<Class>());

    string message = "";
    if (args.size() == 2) {
        if (!checkInstanceOf(args[1], String::ObjectClass, resultOut))
            return false;
        message = args[1].toObject()->as<String>()->value();
    }

    resultOut = gc.create<Exception>(cls, message);
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
    ostringstream s;
    s << className();
    string message = this->message();
    if (message != "")
        s << ": " << message;
    if (pos_.file != "" || pos_.line != 0) {
        s << " at";
        if (pos_.file != "")
            s << " " << pos_.file;
        s << " line " << pos_.line;
    }
    return s.str();
}

void Exception::print(ostream& os) const
{
    os << fullMessage();
}

void Exception::setPos(const TokenPos& pos)
{
    // todo: I get a crash if I try to assign the filename string
    pos_.line = pos.line;
}

bool checkInstanceOf(Traced<Value> v, Traced<Class*> cls, Root<Value>& resultOut)
{
    if (!v.isInstanceOf(cls)) {
        string message = "Expecting " + cls->name() +
            " but got " + v.type()->name();
        resultOut = gc.create<Exception>("TypeError", message);
        return false;
    }

    return true;
}

#ifdef BUILD_TESTS

#include "test.h"

testcase(exception)
{
    Root<Exception*> exc;
    exc = gc.create<Exception>("foo", "bar");
    testEqual(exc->className(), "foo");
    testEqual(exc->message(), "bar");
    testEqual(exc->fullMessage(), "foo: bar");
    exc->setPos(TokenPos("", 1, 0));
    testEqual(exc->fullMessage(), "foo: bar at line 1");
}

#endif
