#include "bool.h"

#include "class.h"

struct BooleanClass : public Class
{
    BooleanClass() : Class("bool") {}
};

BooleanClass* Boolean::ObjectClass = nullptr;
Boolean* Boolean::True = nullptr;
Boolean* Boolean::False = nullptr;

void Boolean::init()
{
    ObjectClass = new BooleanClass;
    Boolean::True = new Boolean(true);
    Boolean::False = new Boolean(false);
}

Boolean::Boolean(bool v)
  : Object(ObjectClass), value_(v)
{}

void Boolean::print(ostream& os) const
{
    os << (value_ ? "True" : "False");
}
