#ifndef __BOOL_H__
#define __BOOL_H__

#include "object.h"
#include "class.h"

struct BooleanClass;

struct Boolean : public Object
{
    static void init();

    static BooleanClass* ObjectClass;

    static Boolean* True;
    static Boolean* False;

    static Boolean* get(bool v) { return v ? True : False; }

    bool value() { return value_; }
    virtual void print(ostream& os) const;

  private:
    Boolean(bool v);

    bool value_;
};

#endif
