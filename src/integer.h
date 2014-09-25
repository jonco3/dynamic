#ifndef __INTEGER_H__
#define __INTEGER_H__

#include "object.h"
#include "class.h"

struct IntegerClass;

struct Integer : public Object
{
    static IntegerClass Class;

    static Value get(int v);

    Integer(int v);
    int toInt() { return value; }
    virtual void print(ostream& os) const;

  private:
    int value;
};

#endif
