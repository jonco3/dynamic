#ifndef __TUPLE_H__
#define __TUPLE_H__

#include "object.h"
#include "class.h"

struct Tuple : public Object
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    static GlobalRoot<Tuple*> Empty;

    static Tuple* get(unsigned size, const Value* values);

    virtual void print(ostream& os) const;
    virtual void traceChildren(Tracer& t);

  private:
    Tuple(unsigned size, const Value* values);

    vector<Value> elements_;
};

#endif
