#ifndef __LIST_H__
#define __LIST_H__

#include "object.h"

struct ListIter;

struct ListBase : public Object
{
    bool len(Root<Value>& resultOut);
    bool getitem(Traced<Value> index, Root<Value>& resultOut);
    bool contains(Traced<Value> element, Root<Value>& resultOut);
    bool iter(Root<Value>& resultOut);

    virtual const string& listName() const = 0;
    virtual void traceChildren(Tracer& t) override;

    size_t len() { return elements_.size(); }
    Traced<Value> getitem(size_t index) {
        return Traced<Value>::fromTracedLocation(&elements_.at(index));
    }

    bool operator==(const ListBase& other);
    bool operator!=(const ListBase& other) { return !(*this == other); }

  protected:
    vector<Value> elements_;
    friend struct ListIter;

    ListBase(Traced<Class*> cls, const TracedVector<Value>& values);
    ListBase(Traced<Class*> cls, size_t size);
};

struct Tuple : public ListBase
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    static GlobalRoot<Tuple*> Empty;

    static Tuple* get(const TracedVector<Value>& values);

    static Tuple* createUninitialised(size_t size);
    void initElement(size_t index, const Value& value);

    virtual const string& listName() const;
    void print(ostream& s) const override;

    Tuple(const TracedVector<Value>& values);
    Tuple(size_t size);
};

struct List : public ListBase
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    List(const TracedVector<Value>& values);

    virtual const string& listName() const;
    virtual void print(ostream& os) const;

    bool setitem(Traced<Value> index, Traced<Value> value, Root<Value>& resultOut);
    bool delitem(Traced<Value> index, Root<Value>& resultOut);
    bool append(Traced<Value> element, Root<Value>& resultOut);
};

extern void initList();

#endif
