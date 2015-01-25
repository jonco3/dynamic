#include "class.h"

GlobalRoot<Class*> Class::ObjectClass;
GlobalRoot<Class*> Class::Null;

void Class::init()
{
    ObjectClass.init(gc::create<Class>("Class"));
    Null.init(nullptr);
    Class::ObjectClass->initClass(Class::ObjectClass, Object::ObjectClass);
    Object::ObjectClass->initClass(Class::ObjectClass, Class::ObjectClass);
}

Class::Class(string name, Traced<Layout*> initialLayout) :
  Object(ObjectClass, initialLayout), name_(name)
{}

void Class::print(ostream& s) const
{
    s << "Class " << name_;
}
