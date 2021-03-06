#include "singletons.h"

#include "object.h"

struct NotImplementedObject : public Object
{
    static GlobalRoot<Class*> ObjectClass;
    static void init();

    NotImplementedObject() : Object(ObjectClass) {}
    void print(ostream& s) const override {
        s << "NotImplemented";
    }
};

struct UninitializedSlotObject : public Object
{
    static GlobalRoot<Class*> ObjectClass;
    static void init();

    UninitializedSlotObject() : Object(ObjectClass) {}
    void print(ostream& s) const override {
        s << "UninitializedSlot";
    }
};

GlobalRoot<Object*> NotImplemented;
GlobalRoot<Object*> UninitializedSlot;
GlobalRoot<Class*> NotImplementedObject::ObjectClass;
GlobalRoot<Class*> UninitializedSlotObject::ObjectClass;

void NotImplementedObject::init()
{
    ObjectClass.init(Class::createNative("NotImplemented", nullptr));
    NotImplemented.init(gc.create<NotImplementedObject>());
}

void UninitializedSlotObject::init()
{
    ObjectClass.init(Class::createNative("UninitializedSlot", nullptr));
    UninitializedSlot.init(gc.create<UninitializedSlotObject>());
}

void initSingletons()
{
    NotImplementedObject::init();
    UninitializedSlotObject::init();
}
