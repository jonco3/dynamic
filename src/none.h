#ifndef __NONE_H__
#define __NONE_H__

#include "object.h"
#include "class.h"

struct NoneObject : public Object
{
    static Class Class;

    NoneObject() : Object(&Class) {}
    virtual void print(ostream& s) const { s << "None"; }
};

struct UninitializedSlotObject : public Object
{
    static Class Class;

    UninitializedSlotObject() : Object(&Class) {}
    virtual void print(ostream& s) const { s << "UninitializedSlot"; }
};

extern UninitializedSlotObject* UninitializedSlot;

#endif
