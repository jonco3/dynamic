#ifndef __CLASS_H__
#define __CLASS_H__

#include "object.h"

#include <vector>

struct Class : public Object
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;
    static GlobalRoot<Class*> Null;

    Class(string name, Traced<Layout*> initialLayout = Object::InitialLayout);

    const string& name() const { return name_; }

    virtual void print(ostream& s) const;

  private:
    string name_;
};

#endif
