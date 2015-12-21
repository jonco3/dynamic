#ifndef __DICT_H__
#define __DICT_H__

#include "object.h"

struct Dict : public Object
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    Dict();
    Dict(const TracedVector<Value>& values);
    Dict(const Traced<Class*> cls);

    void traceChildren(Tracer& t) override;
    void print(ostream& s) const override;

    bool len(MutableTraced<Value> resultOut);
    bool contains(Traced<Value> key, MutableTraced<Value> resultOut);
    bool getitem(Traced<Value> key, MutableTraced<Value> resultOut);
    void setitem(Traced<Value> key, Traced<Value> value);
    bool delitem(Traced<Value> key, MutableTraced<Value> resultOut);
    bool keys(MutableTraced<Value> resultOut);
    bool values(MutableTraced<Value> resultOut);

    // Methods __eq__ and __ne__ are supplied by lib/internals.py

  private:
    struct ValueHash {
        size_t operator()(Value v) const;
    };

    struct ValuesEqual {
        bool operator()(Value a, Value b) const;
    };

    unordered_map<Value, Heap<Value>, ValueHash, ValuesEqual> entries_;
};

#endif
