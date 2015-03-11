#include "callable.h"

#include "object.h"

GlobalRoot<Class*> Native::ObjectClass;

void Native::init()
{
    ObjectClass.init(gc::create<Class>("Native"));
}

GlobalRoot<Class*> Function::ObjectClass;

void Function::init()
{
    ObjectClass.init(gc::create<Class>("Function"));
}

Function::Function(const vector<Name>& params,
                   Traced<Block*> block,
                   Traced<Frame*> scope,
                   bool isGenerator)
  : Callable(ObjectClass),
    params_(params),
    block_(block),
    scope_(scope),
    isGenerator_(isGenerator)
{}

//here
