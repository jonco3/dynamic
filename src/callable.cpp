#include "callable.h"

#include "class.h"

Class* Native::ObjectClass;

void Native::init()
{
    gc::addRoot(&ObjectClass);
    ObjectClass = new Class("Native");
}

Class* Function::ObjectClass;

void Function::init()
{
    gc::addRoot(&ObjectClass);
    ObjectClass = new Class("Function");
}

//here
