#ifndef __DICT_H__
#define __DICT_H__

#include "object.h"

struct Dict : public Object
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    Dict();
    Dict(const TracedVector<Value>& values);

    void traceChildren(Tracer& t) override;
    void print(ostream& s) const override;

    bool len(Root<Value>& resultOut);
    bool contains(Traced<Value> key, Root<Value>& resultOut);
    bool getitem(Traced<Value> key, Root<Value>& resultOut);
    bool setitem(Traced<Value> key, Traced<Value> value, Root<Value>& resultOut);
    bool delitem(Traced<Value> key, Root<Value>& resultOut);
    bool keys(Root<Value>& resultOut);

    // Methods __eq__ and __ne__ are supplied by lib/internals.py

  private:
    struct ValueHash {
        size_t operator()(Value v) const;
    };

    struct ValuesEqual {
        bool operator()(Value a, Value b) const;
    };

    unordered_map<Value, Value, ValueHash, ValuesEqual> entries_;
};

#endif
