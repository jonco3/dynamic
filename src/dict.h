#ifndef __DICT_H__
#define __DICT_H__

#include "object.h"
#include "class.h"

struct Dict : public Object
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    Dict(unsigned size, const Value* values);

    virtual void traceChildren(Tracer& t);
    virtual void print(ostream& os) const;

    bool getitem(Traced<Value> key, Root<Value>& resultOut);
    bool setitem(Traced<Value> key, Traced<Value> value, Root<Value>& resultOut);

  private:
    unordered_map<Value, Value> entries_;
};

#endif