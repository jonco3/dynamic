#include "class.h"

GlobalRoot<Class*> Class::ObjectClass;

void Class::init()
{
    ObjectClass.init(new Class("Class"));
    Class::ObjectClass->initClass(Class::ObjectClass, Object::Null);
    Object::ObjectClass->initClass(Class::ObjectClass, Object::Null);
}

Class::Class(string name, Traced<Layout*> initialLayout) :
  Object(ObjectClass, initialLayout), name_(name)
{}

void Class::print(ostream& s) const
{
    s << "Class " << name_;
}
