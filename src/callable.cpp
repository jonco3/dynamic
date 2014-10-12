#include "callable.h"

#include "class.h"

Root<Class> Native::ObjectClass;

void Native::init()
{
    ObjectClass = new Class("Native");
}

Root<Class> Function::ObjectClass;

void Function::init()
{
    ObjectClass = new Class("Function");
}

//here
