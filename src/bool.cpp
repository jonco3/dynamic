#include "bool.h"

#include "object.h"

GlobalRoot<Class*> Boolean::ObjectClass;
GlobalRoot<Boolean*> Boolean::True;
GlobalRoot<Boolean*> Boolean::False;

void Boolean::init()
{
    ObjectClass.init(gc.create<Class>("bool"));
    True.init(gc.create<Boolean>(true));
    False.init(gc.create<Boolean>(false));
}

Boolean::Boolean(bool v)
  : Object(ObjectClass), value_(v)
{}

void Boolean::print(ostream& os) const
{
    os << (value_ ? "True" : "False");
}
