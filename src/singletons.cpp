#include "singletons.h"

#include "object.h"

struct NotImplementedObject : public Object
{
    static GlobalRoot<Class*> ObjectClass;
    static void init();

    NotImplementedObject() : Object(ObjectClass) {}
    virtual void print(ostream& s) const { s << "NotImplemented"; }
};

struct UninitializedSlotObject : public Object
{
    static GlobalRoot<Class*> ObjectClass;
    static void init();

    UninitializedSlotObject() : Object(ObjectClass) {}
    virtual void print(ostream& s) const { s << "UninitializedSlot"; }
};

GlobalRoot<Object*> NotImplemented;
GlobalRoot<Object*> UninitializedSlot;
GlobalRoot<Class*> NotImplementedObject::ObjectClass;
GlobalRoot<Class*> UninitializedSlotObject::ObjectClass;

void NotImplementedObject::init()
{
    ObjectClass.init(gc.create<Class>("NotImplemented"));
    NotImplemented.init(gc.create<NotImplementedObject>());
}

void UninitializedSlotObject::init()
{
    ObjectClass.init(gc.create<Class>("UninitializedSlot"));
    UninitializedSlot.init(gc.create<UninitializedSlotObject>());
}

void initSingletons()
{
    NotImplementedObject::init();
    UninitializedSlotObject::init();
}
