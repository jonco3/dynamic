#include "callable.h"

#include "exception.h"
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
    assert(!takesRest || params_.size() > 0);
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

bool checkInstanceOf(Traced<Value> v, Traced<Class*> cls,
                     Root<Value>& resultOut)
{
    if (!v.isInstanceOf(cls)) {
        string message = "Expecting " + cls->name() +
            " but got " + v.type()->name();
        resultOut = gc.create<TypeError>(message);
        return false;
    }

    return true;
}
