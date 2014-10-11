#include "none.h"

#include "class.h"

Object* None = nullptr;
Object* UninitializedSlot = nullptr;

struct NoneObject : public Object
{
    static Class* ObjectClass;
    static void init();

    NoneObject() : Object(ObjectClass) {}
    virtual void print(ostream& s) const { s << "None"; }
};

struct UninitializedSlotObject : public Object
{
    static Class* ObjectClass;
    static void init();

    UninitializedSlotObject() : Object(ObjectClass) {}
    virtual void print(ostream& s) const { s << "UninitializedSlot"; }
};

Class* NoneObject::ObjectClass = nullptr;
Class* UninitializedSlotObject::ObjectClass = nullptr;

void NoneObject::init()
{
    gc::addRoot(&ObjectClass);
    gc::addRoot(&None);
    ObjectClass = new Class("None");
    None = new NoneObject;
}

void UninitializedSlotObject::init()
{
    gc::addRoot(&ObjectClass);
    gc::addRoot(&UninitializedSlot);
    ObjectClass = new Class("UninitializedSlot");
    UninitializedSlot = new UninitializedSlotObject;
}

void initSingletons()
{
    NoneObject::init();
    UninitializedSlotObject::init();
}
