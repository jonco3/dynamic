#include "exception.h"

#include "callable.h"
#include "singletons.h"
#include "string.h"
#include "test.h"

#include "value-inl.h"

#include <sstream>

GlobalRoot<Class*> Exception::ObjectClass;

#define define_exception_class(name)                                          \
    GlobalRoot<Class*> name::ObjectClass;
for_each_exception_class(define_exception_class)
#undef define_exception_class

GlobalRoot<StopIteration*> StopIterationException;

template <typename T>
static bool exception_new(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    assert(args.size() >= 1 && args.size() <= 2);
    if (!checkInstanceOf(args[0], Class::ObjectClass, resultOut))
        return false;
    Stack<Class*> cls(args[0].asObject()->as<Class>());

    Stack<String*> message(String::EmptyString);
    if (args.size() == 2) {
        if (!checkInstanceOf(args[1], String::ObjectClass, resultOut))
            return false;
        message = args[1].asObject()->as<String>();
    }

    resultOut = gc.create<T>(cls, message->value());
    return true;
}

void Exception::init()
{
    ObjectClass.init(Class::createNative("Exception",
                                         exception_new<Exception>, 2));

#define init_exception_class(name)                                          \
    name::ObjectClass.init(                                                 \
        Class::createNative(#name,                                          \
                            exception_new<name>, 2,                         \
                            Exception::ObjectClass));                       \

    for_each_exception_class(init_exception_class)
#undef init_exception_class

    StopIterationException.init(gc.create<StopIteration>());
}

Exception::Exception(Traced<Class*> cls, const string& message)
  : Object(cls)
{
    assert(cls->isDerivedFrom(ObjectClass));
    Stack<String*> str(String::get(message));
    init(str);
}

Exception::Exception(Traced<Class*> cls, Traced<String*> message)
  : Object(cls)
{
    assert(cls->isDerivedFrom(ObjectClass));
    init(message);
}

void Exception::init(Traced<String*> message)
{
    maybeAbortTests(className() + " " + message->value());
    setAttr(Name::message, message);
}

string Exception::className() const
{
    return type()->name();
}

string Exception::message() const
{
    Stack<Value> value(getAttr(Name::message));
    return value.get().as<String>()->value();
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
    assert(!hasPos());
    pos_ = pos;
}
