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

    size_t len() const {
        return entries_.size();
    }

    bool contains(Traced<Value> key) const {
        return entries_.find(key) != entries_.end();
    }

    bool getitem(Traced<Value> key, MutableTraced<Value> resultOut) const;
    void setitem(Traced<Value> key, Traced<Value> value);
    bool delitem(Traced<Value> key, MutableTraced<Value> resultOut);
    Value keys() const;
    Value values() const;

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

// An adaptor to access the slots of an object as a dict.
struct DictView : public Object
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    DictView(Traced<Object*> obj);

    void traceChildren(Tracer& t) override;
    void print(ostream& s) const override;

    size_t len() const;
    bool contains(Traced<Value> key) const;
    bool getitem(Traced<Value> key, MutableTraced<Value> resultOut) const;
    void setitem(Traced<Value> key, Traced<Value> value);
    bool delitem(Traced<Value> key, MutableTraced<Value> resultOut);
    Value keys() const;
    Value values() const;

  private:
    Heap<Object*> object_;
    Heap<Layout*> layout_;
    mutable unordered_map<Name, size_t> slots_;

    static const int NotFound = Layout::NotFound;
    static const int BadName = -2;

    void checkMap() const;
    int findSlot(Traced<Value> key) const;
    int findSlot(Name Name) const;
};

#endif
