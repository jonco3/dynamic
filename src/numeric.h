#ifndef __NUMERIC_H__
#define __NUMERIC_H__

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
    void print(ostream& s) const override;

  private:
    int value_;
};

struct Float : public Object
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    static Float* get(double v);

    Float(double v);
    void print(ostream& s) const override;

    const double value;
};

#endif
