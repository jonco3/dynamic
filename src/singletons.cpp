#include "singletons.h"

#include "class.h"

struct NoneObject : public Object
{
    static GlobalRoot<Class*> ObjectClass;
    static void init();

    NoneObject() : Object(ObjectClass) {}
    virtual void print(ostream& s) const { s << "None"; }
};

struct UninitializedSlotObject : public Object
{
    static GlobalRoot<Class*> ObjectClass;
    static void init();

    UninitializedSlotObject() : Object(ObjectClass) {}
    virtual void print(ostream& s) const { s << "UninitializedSlot"; }
};

GlobalRoot<Object*> None;
GlobalRoot<Object*> UninitializedSlot;
GlobalRoot<Class*> NoneObject::ObjectClass;
GlobalRoot<Class*> UninitializedSlotObject::ObjectClass;

void NoneObject::init()
{
    ObjectClass.init(new Class("None"));
    None.init(new NoneObject);
}

void UninitializedSlotObject::init()
{
    ObjectClass.init(new Class("UninitializedSlot"));
    UninitializedSlot.init(new UninitializedSlotObject);
}

void initSingletons()
{
    NoneObject::init();
    UninitializedSlotObject::init();
}
