#ifndef __BOOL_H__
#define __BOOL_H__

#include "object.h"
#include "class.h"

struct Boolean : public Object
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    static GlobalRoot<Boolean*> True;
    static GlobalRoot<Boolean*> False;

    static Boolean* get(bool v) { return v ? True : False; }

    bool value() { return value_; }
    virtual void print(ostream& os) const;

    Boolean(bool v);

  private:
    bool value_;
};

#endif
