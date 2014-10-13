#include "callable.h"

#include "class.h"

GlobalRoot<Class*> Native::ObjectClass;

void Native::init()
{
    ObjectClass.init(new Class("Native"));
}

GlobalRoot<Class*> Function::ObjectClass;

void Function::init()
{
    ObjectClass.init(new Class("Function"));
}

//here
