#ifndef __BOOL_H__
#define __BOOL_H__

#include "object.h"

struct Boolean : public Object
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    static GlobalRoot<Boolean*> True;
    static GlobalRoot<Boolean*> False;

    static Boolean* get(bool v) { return v ? True : False; }

    bool value() { return value_; }
    void print(ostream& s) const override;

    Boolean(bool v);

  private:
    bool value_;
};

#endif
