#include "class.h"

GlobalRoot<Class*> Class::ObjectClass;

void Class::init()
{
    ObjectClass.init(new Class("Class"));
    Class::ObjectClass->initClass(Class::ObjectClass, Object::Null);
    Object::ObjectClass->initClass(Class::ObjectClass, Object::Null);
}

Class::Class(string name) :
  Object(ObjectClass, Object::Null, Object::InitialLayout), name_(name)
{}

void Class::print(ostream& s) const
{
    s << "Class " << name_;
}
