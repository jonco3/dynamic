#include "bool.h"

#include "class.h"

struct BooleanClass : public Class
{
    BooleanClass() : Class("bool") {}
};

Root<Class> Boolean::ObjectClass;
Root<Boolean> Boolean::True;
Root<Boolean> Boolean::False;

void Boolean::init()
{
    ObjectClass = new BooleanClass;
    True = new Boolean(true);
    False = new Boolean(false);
}

Boolean::Boolean(bool v)
  : Object(ObjectClass), value_(v)
{}

void Boolean::print(ostream& os) const
{
    os << (value_ ? "True" : "False");
}
