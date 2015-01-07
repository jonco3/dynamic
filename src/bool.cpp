#include "bool.h"

#include "class.h"

GlobalRoot<Class*> Boolean::ObjectClass;
GlobalRoot<Boolean*> Boolean::True;
GlobalRoot<Boolean*> Boolean::False;

void Boolean::init()
{
    ObjectClass.init(new Class("bool"));
    True.init(new Boolean(true));
    False.init(new Boolean(false));
}

Boolean::Boolean(bool v)
  : Object(ObjectClass), value_(v)
{}

void Boolean::print(ostream& os) const
{
    os << (value_ ? "True" : "False");
}
