#include "callable.h"

#include "class.h"

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

//here
