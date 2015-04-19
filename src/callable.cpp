#include "callable.h"

#include "object.h"

#include <climits>

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

FunctionInfo::FunctionInfo(const vector<Name>& paramNames, Traced<Block*> block,
                           unsigned defaultCount, bool takesRest,
                           bool isGenerator)
  : params_(paramNames),
    block_(block),
    defaultCount_(defaultCount),
    takesRest_(takesRest),
    isGenerator_(isGenerator)
{
    assert(!takesRest || params_.size() > 0);
}

void Function::init()
{
    ObjectClass.init(gc::create<Class>("Function"));
}

Function::Function(Name name,
                   Traced<FunctionInfo*> info,
                   TracedVector<Value> defaults,
                   Traced<Frame*> scope)
  : Callable(ObjectClass, name,
             info->paramCount() - defaults.size() - (info->takesRest_ ? 1 : 0),
             info->takesRest_ ? UINT_MAX : info->paramCount()),
    info_(info),
    scope_(scope)
{
    assert(info->paramCount() >= defaults.size());
    for (size_t i = 0; i < defaults.size(); i++)
        defaults_.push_back(defaults[i]);
}

void initNativeMethod(Traced<Object*> cls, Name name, Native::Func func,
                      unsigned minArgs, unsigned maxArgs)
{
    Root<Value> value(gc::create<Native>(name, func, minArgs, maxArgs));
    cls->setAttr(name, value);
}
