#include "class.h"

GlobalRoot<Class> Class::ObjectClass;

void Class::init()
{
    ObjectClass.init(new Class("Class"));
    Class::ObjectClass->initClass(Class::ObjectClass, nullptr);
    Object::ObjectClass->initClass(Class::ObjectClass, nullptr);
}

Class::Class(string name) :
  Object(ObjectClass, nullptr, Object::InitialLayout), name_(name)
{}

void Class::print(ostream& s) const
{
    s << "Class " << name_;
}
