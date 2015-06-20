#ifndef __NUMERIC_H__
#define __NUMERIC_H__

#include "object.h"

struct Integer : public Object
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    static Value get(int64_t v);
    static Object* getObject(int64_t v);

    Integer(int64_t v = 0);
    Integer(Traced<Class*> cls);

    int64_t value() const { return value_; }
    void print(ostream& s) const override;

  private:
    int64_t value_;
};

struct Float : public Object
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    static Value get(double v);
    static Object* getObject(double v);

    Float(double v = 0.0);
    Float(Traced<Class*> cls);

    double value() const { return value_; }
    void print(ostream& s) const override;

  private:
    const double value_;
};

#endif
