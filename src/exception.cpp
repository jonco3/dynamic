#include "exception.h"

#include "callable.h"
#include "instr.h"
#include "interp.h"
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
static bool exception_new(NativeArgs args, MutableTraced<Value> resultOut)
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
    setAttr(Names::message, message);
}

string Exception::className() const
{
    return type()->name();
}

string Exception::message() const
{
    Stack<Value> value(getAttr(Names::message));
    return value.get().as<String>()->value();
}

string Exception::fullMessage() const
{
    ostringstream s;
    s << className();
    string message = this->message();
    if (message != "")
        s << ": " << message;
    return s.str();
}

string Exception::traceback() const
{
    ostringstream s;
    s << "Traceback (most recent call last):" << endl;
    for (size_t i = traceback_.size(); i != 0; i--) {
        TokenPos pos = traceback_[i - 1];
        if (pos.file != "" || pos.line != 0) {
            s << "  File \"" << pos.file << "\", ";
            s << "line " << pos.line << endl;
        }
    }
    return s.str();
}

void Exception::print(ostream& s) const
{
    s << fullMessage();
}

void Exception::recordTraceback(const InstrThunk* instrp)
{
    assert(traceback_.empty());
    size_t count = interp->frameCount();
    traceback_.reserve(count);
    for (size_t i = 0; i < count; i++) {
        Frame* frame = interp->getFrame(i);
        if (instrp)
            traceback_.push_back(frame->block()->getPos(instrp));
        instrp = frame->returnPoint();
    }
}
