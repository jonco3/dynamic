#include "singletons.h"

#include "callable.h"
#include "class.h"
#include "integer.h"
#include "value-inl.h"

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
GlobalRoot<Object*> Builtin;
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

static Value builtin_print(Value v)
{
    printf("%d\n", v.toObject()->as<Integer>()->value());
    return None;
}

static void initBuiltins()
{
    Builtin.init(new Object);
    Root<Value> value;
    value = new Native1(builtin_print);
    Builtin->setAttr("print", value);
}

void initSingletons()
{
    NoneObject::init();
    UninitializedSlotObject::init();
    initBuiltins();
}
