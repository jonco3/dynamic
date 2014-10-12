#include "none.h"

#include "class.h"

Root<Object> None;
Root<Object> UninitializedSlot;

struct NoneObject : public Object
{
    static Root<Class> ObjectClass;
    static void init();

    NoneObject() : Object(ObjectClass) {}
    virtual void print(ostream& s) const { s << "None"; }
};

struct UninitializedSlotObject : public Object
{
    static Root<Class> ObjectClass;
    static void init();

    UninitializedSlotObject() : Object(ObjectClass) {}
    virtual void print(ostream& s) const { s << "UninitializedSlot"; }
};

Root<Class> NoneObject::ObjectClass;
Root<Class> UninitializedSlotObject::ObjectClass;

void NoneObject::init()
{
    ObjectClass = new Class("None");
    None = new NoneObject;
}

void UninitializedSlotObject::init()
{
    ObjectClass = new Class("UninitializedSlot");
    UninitializedSlot = new UninitializedSlotObject;
}

void initSingletons()
{
    NoneObject::init();
    UninitializedSlotObject::init();
}
