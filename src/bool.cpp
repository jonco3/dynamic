#include "bool.h"

#include "object.h"
#include "value-inl.h"

GlobalRoot<Class*> Boolean::ObjectClass;
GlobalRoot<Boolean*> Boolean::True;
GlobalRoot<Boolean*> Boolean::False;

static bool boolNew(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], Class::ObjectClass, resultOut))
        return false;

    if (args.size() == 1) {
        resultOut = Boolean::False;
        return true;
    }

    // todo: convert argument to bool

    return true;
}

void Boolean::init()
{
    ObjectClass.init(Class::createNative("bool", boolNew));
    True.init(gc.create<Boolean>(true));
    False.init(gc.create<Boolean>(false));
}

Boolean::Boolean(bool v)
  : Object(ObjectClass), value_(v)
{}

void Boolean::print(ostream& s) const
{
    s << (value_ ? "True" : "False");
}
