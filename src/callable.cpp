#include "callable.h"

#include "class.h"

Class* Native::ObjectClass;

void Native::init()
{
    ObjectClass = new Class("Native");
}

Class* Function::ObjectClass;

void Function::init()
{
    ObjectClass = new Class("Function");
}

//here
