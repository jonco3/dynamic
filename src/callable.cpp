#include "callable.h"

#include "object.h"

GlobalRoot<Class*> Native::ObjectClass;
GlobalRoot<Class*> Function::ObjectClass;

Callable::Callable(Traced<Class*> cls, unsigned reqArgs)
  : Object(cls), reqArgs_(reqArgs)
{}

void Native::init()
{
    ObjectClass.init(gc::create<Class>("Native"));
}

Native::Native(unsigned reqArgs, Func func)
  : Callable(ObjectClass, reqArgs),
    func_(func)
{}

void Function::init()
{
    ObjectClass.init(gc::create<Class>("Function"));
}

Function::Function(const vector<Name>& params,
                   Traced<Block*> block,
                   Traced<Frame*> scope,
                   bool isGenerator)
  : Callable(ObjectClass, params.size()),
    params_(params),
    block_(block),
    scope_(scope),
    isGenerator_(isGenerator)
{}

//here
