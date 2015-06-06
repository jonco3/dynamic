#include "exception.h"

#include "callable.h"
#include "singletons.h"
#include "string.h"
#include "test.h"

#include <sstream>

GlobalRoot<Class*> Exception::ObjectClass;

#define define_exception_class(name)                                          \
    GlobalRoot<Class*> name::ObjectClass;
for_each_exception_class(define_exception_class)
#undef define_exception_class

GlobalRoot<StopIteration*> StopIterationException;

static bool exceptionInit(TracedVector<Value> args, Root<Value>& resultOut)
{
    return args[0].toObject()->as<Exception>()->init(args, resultOut);
}

void Exception::init()
{
    ObjectClass.init(Class::createNative<Exception>("Exception"));
    initNativeMethod(ObjectClass, "__init__", exceptionInit, 1, 2);

#define init_exception_class(name)                                          \
    name::ObjectClass.init(                                                 \
        Class::createNative<name>(#name, Exception::ObjectClass));          \
    initNativeMethod(name::ObjectClass, "__init__", exceptionInit, 1, 2);

    for_each_exception_class(init_exception_class)
#undef init_exception_class

    StopIterationException.init(gc.create<StopIteration>());
}

Exception::Exception(Traced<Class*> cls)
  : Object(cls)
{
    assert(cls->isDerivedFrom(ObjectClass));
    setAttr("message", String::EmptyString);
}

Exception::Exception(Traced<Class*> cls, const string& message)
  : Object(cls)
{
    assert(cls->isDerivedFrom(ObjectClass));
    Root<Value> messageValue(String::get(message));
    init(messageValue);
}

bool Exception::init(TracedVector<Value> args, Root<Value>& resultOut)
{
    assert(args.size() >= 1 && args.size() <= 2);

    Root<Value> message(String::EmptyString);
    if (args.size() == 2) {
        if (!checkInstanceOf(args[1], String::ObjectClass, resultOut))
            return false;
        message = args[1];
    }
    init(message);

    resultOut = None;
    return true;
}

void Exception::init(Traced<Value> message)
{
    assert(message.asObject()->is<String>()); // todo: check
    maybeAbortTests(
        className() + " " + message.asObject()->as<String>()->value());
    setAttr("message", message);
}

string Exception::className() const
{
    return type()->name();
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

void Exception::print(ostream& s) const
{
    s << fullMessage();
}

void Exception::setPos(const TokenPos& pos)
{
    if (pos_.line != 0)
        return;
    pos_ = pos;
}
