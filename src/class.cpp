#include "class.h"

Class* Class::ObjectClass = nullptr;

void Class::init()
{
    ObjectClass = new Class("Class");
    Class::ObjectClass->initClass(Class::ObjectClass, nullptr);
    Object::ObjectClass->initClass(Class::ObjectClass, nullptr);
}

Class::Class(string name) :
  Object(ObjectClass, nullptr, Object::InitialLayout), name_(name)
{}
