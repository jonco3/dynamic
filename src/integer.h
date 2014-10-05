#ifndef __INTEGER_H__
#define __INTEGER_H__

#include "object.h"
#include "class.h"

struct IntegerClass;

struct Integer : public Object
{
    static void init();

    static IntegerClass* ObjectClass;

    static Integer* Zero;

    static Value get(int v);

    Integer(int v);
    int value() { return value_; }
    virtual void print(ostream& os) const;

  private:
    int value_;
};

#endif
