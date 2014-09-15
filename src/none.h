#ifndef __NONE_H__
#define __NONE_H__

#include "object.h"
#include "class.h"

struct NoneObject : public Object
{
    static Class Class;

    NoneObject() : Object(&Class) {}
    virtual void print(std::ostream& s) const { s << "None"; }
};

extern NoneObject* None;

#endif
