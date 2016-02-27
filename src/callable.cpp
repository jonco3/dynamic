#include "callable.h"

#include "block.h"
#include "exception.h"
#include "frame.h"
#include "object.h"

#include "value-inl.h"

#include <climits>

GlobalRoot<Class*> Native::ObjectClass;
GlobalRoot<Class*> Function::ObjectClass;
GlobalRoot<Class*> Method::ObjectClass;

Callable::Callable(Traced<Class*> cls, Name name,
                   unsigned minArgs, unsigned maxArgs)
  : Object(cls),
    name_(name),
    minArgs_(minArgs),
    maxArgs_(maxArgs)
{}

void Callable::print(ostream& s) const
{
    s << "<" << type()->name();
    s << " object '" << *name_;
    s << "' at 0x" << hex << reinterpret_cast<uintptr_t>(this) << ">";
}

static bool callable_get(NativeArgs args, MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], Callable::ObjectClass, resultOut))
        return false;

    Stack<Callable*> callable(args[0].asObject()->as<Callable>());
    Stack<Object*> instance(args[1].toObject());
    if (instance == None)
        resultOut = Value(callable);
    else
        resultOut = gc.create<Method>(callable, instance);
    return true;
}

Native::Native(Name name, NativeFunc func, unsigned minArgs, unsigned maxArgs)
  : Callable(ObjectClass, name, minArgs, maxArgs ? maxArgs : minArgs),
    func_(func)
{}

FunctionInfo::FunctionInfo(const vector<Name>& paramNames, Traced<Block*> block,
                           unsigned defaultCount, bool takesRest,
                           bool isGenerator)
  : params_(paramNames),
    block_(block),
    defaultCount_(defaultCount),
    takesRest_(takesRest),
    isGenerator_(isGenerator)
{
    assert(!takesRest || argCount() > 0);
    assert(argCount() == block->argCount());
}

void FunctionInfo::traceChildren(Tracer& t)
{
    gc.trace(t, &block_);
}

Function::Function(Name name,
                   Traced<FunctionInfo*> info,
                   TracedVector<Value> defaults,
                   Traced<Env*> env)
  : Callable(ObjectClass, name,
             info->argCount() - defaults.size() - (info->takesRest_ ? 1 : 0),
             info->takesRest_ ? UINT_MAX : info->argCount()),
    info_(info),
    env_(env)
{
    assert(info->argCount() >= defaults.size());
    for (size_t i = 0; i < defaults.size(); i++)
        defaults_.push_back(defaults[i]);
}

void Function::traceChildren(Tracer& t)
{
    Callable::traceChildren(t);
    gc.trace(t, &info_);
    gc.traceVector(t, &defaults_);
    gc.trace(t, &env_);
}

void Function::dump(ostream& s) const
{
    s << type()->name();
    s << " object at 0x" << hex << reinterpret_cast<uintptr_t>(this) << endl;
    s << *info_->block_ << endl;
}

Method::Method(Traced<Callable*> callable, Traced<Object*> object)
  : Object(ObjectClass), callable_(callable), object_(object)
{}

void Method::traceChildren(Tracer& t)
{
    Object::traceChildren(t);
    gc.trace(t, &callable_);
    gc.trace(t, &object_);
}

void initCallable()
{
    // Classes for Native and Function are initialized in initObject().
    Method::ObjectClass.init(Class::createNative("method", nullptr));

    initNativeMethod(Native::ObjectClass, "__get__", callable_get, 3, 3);
    initNativeMethod(Function::ObjectClass, "__get__", callable_get, 3, 3);
}

bool checkInstanceOf(Traced<Value> v, Traced<Class*> cls,
                     MutableTraced<Value> resultOut)
{
    if (!v.isInstanceOf(cls)) {
        string message = "Expecting " + cls->name() +
            " but got " + v.type()->name();
        resultOut = gc.create<TypeError>(message);
        return false;
    }

    return true;
}
