#ifndef __INTEGER_H__
#define __INTEGER_H__

#include "object.h"

struct Integer : public Object
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    static GlobalRoot<Integer*> Zero;

    static Value get(int v);
    static Object* getObject(int v);

    Integer(int v);
    int value() { return value_; }
    virtual void print(ostream& os) const;

  private:
    int value_;
};

#endif
