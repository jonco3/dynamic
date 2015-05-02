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
    ObjectClass.init(gc.create<Class>("Exception",
                                      Object::ObjectClass,
                                      Exception::createInstance));
    initNativeMethod(ObjectClass, "__init__", exceptionInit, 1, 2);

#define init_exception_class(name)                                          \
    name::ObjectClass.init(                                                 \
        gc.create<Class>(#name,                                             \
                         Exception::ObjectClass,                            \
                         Exception::createInstance));                       \
    initNativeMethod(name::ObjectClass, "__init__", exceptionInit, 1, 2);
    for_each_exception_class(init_exception_class)
#undef init_exception_class

    StopIterationException.init(gc.create<StopIteration>());
}

Exception::Exception(Traced<Class*> cls)
  : Object(cls)
{
    assert(cls->is<Class>()); // todo: check derives from Exception
}

Exception::Exception(Traced<Class*> cls, const string& message)
  : Object(cls)
{
    assert(cls->is<Class>());
    Root<Value> messageValue(String::get(message));
    init(messageValue);
}

Object* Exception::createInstance(Traced<Class*> cls)
{
    // todo: check class has exception as a base
    return gc.create<Exception>(cls);
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
        resultOut = gc.create<TypeError>(message);
        return false;
    }

    return true;
}

#ifdef BUILD_TESTS

#include "test.h"

testcase(exception)
{
    Root<Exception*> exc;
    testExpectingException = true;
    exc = gc.create<Exception>(Exception::ObjectClass, "bar");
    testEqual(exc->className(), "Exception");
    testEqual(exc->message(), "bar");
    testEqual(exc->fullMessage(), "Exception: bar");
    exc->setPos(TokenPos("", 1, 0));
    testEqual(exc->fullMessage(), "Exception: bar at line 1");

    exc = gc.create<Exception>(TypeError::ObjectClass, "baz");
    testEqual(exc->fullMessage(), "TypeError: baz");

    exc = Exception::createInstance(TypeError::ObjectClass)->as<Exception>();
    RootVector<Value> args(2);
    args[0] = TypeError::ObjectClass;
    args[1] = String::get("foo");
    Root<Value> result;
    testTrue(exc->init(args, result));
    testEqual(result.toObject(), None.get());
    testEqual(exc->fullMessage(), "TypeError: foo");

    testExpectingException = false;
}

#endif
