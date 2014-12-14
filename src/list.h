#ifndef __LIST_H__
#define __LIST_H__

#include "object.h"
#include "class.h"

struct ListBase : public Object
{
    ListBase(Traced<Class*> cls, const TracedVector<Value>& values);

    bool getitem(Traced<Value> index, Root<Value>& resultOut);
    bool contains(Traced<Value> element, Root<Value>& resultOut);

    virtual const string& listName() const = 0;
    virtual void traceChildren(Tracer& t);

  protected:
    vector<Value> elements_;
};

struct Tuple : public ListBase
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    static GlobalRoot<Tuple*> Empty;

    static Tuple* get(const TracedVector<Value>& values);

    virtual const string& listName() const;
    virtual void print(ostream& os) const;

  private:
    Tuple(const TracedVector<Value>& values);
};

struct List : public ListBase
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    List(const TracedVector<Value>& values);

    virtual const string& listName() const;
    virtual void print(ostream& os) const;

    bool setitem(Traced<Value> index, Traced<Value> value, Root<Value>& resultOut);
};

#endif
