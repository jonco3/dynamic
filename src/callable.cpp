#include "callable.h"

#include "object.h"

GlobalRoot<Class*> Native::ObjectClass;
GlobalRoot<Class*> Function::ObjectClass;

Callable::Callable(Traced<Class*> cls, Name name,
                   unsigned minArgs, unsigned maxArgs)
  : Object(cls),
    name_(name),
    minArgs_(minArgs),
    maxArgs_(maxArgs)
{}

void Native::init()
{
    ObjectClass.init(gc::create<Class>("Native"));
}

Native::Native(Name name, Func func, unsigned minArgs, unsigned maxArgs)
  : Callable(ObjectClass, name, minArgs, maxArgs ? maxArgs : minArgs),
    func_(func)
{}

void Function::init()
{
    ObjectClass.init(gc::create<Class>("Function"));
}

Function::Function(Name name,
                   const vector<Name>& params,
                   TracedVector<Value> defaults,
                   Traced<Block*> block,
                   Traced<Frame*> scope,
                   bool isGenerator)
  : Callable(ObjectClass, name,
             params.size() - defaults.size(), params.size()),
    params_(params),
    block_(block),
    scope_(scope),
    isGenerator_(isGenerator)
{
    assert(params.size() >= defaults.size());
    for (size_t i = 0; i < defaults.size(); i++)
        defaults_.push_back(defaults[i]);
}

void initNativeMethod(Traced<Object*> cls, Name name, Native::Func func,
                      unsigned minArgs, unsigned maxArgs)
{
    Root<Value> value(gc::create<Native>(name, func, minArgs, maxArgs));
    cls->setAttr(name, value);
}

//here
