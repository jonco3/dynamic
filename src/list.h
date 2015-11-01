#ifndef __LIST_H__
#define __LIST_H__

#include "object.h"

struct List;

struct Tuple : public Object
{
    static GlobalRoot<Class*> ObjectClass;
    static GlobalRoot<Tuple*> Empty;

    static void init();

    static Tuple* get(const TracedVector<Value>& values);
    Tuple(const TracedVector<Value>& values);
    Tuple(size_t size);
    Tuple(Traced<Class*> cls, size_t size);
    Tuple(Traced<Class*> cls, Traced<Tuple*> init);
    Tuple(Traced<Class*> cls, Traced<List*> init);
    void initElement(size_t index, const Value& value);

    void print(ostream& s) const override;
    void traceChildren(Tracer& t) override;

    int32_t len() { return (int32_t)elements_.size(); }
    bool contains(Traced<Value> value);
    Value getitem(size_t index) { return elements_.at(index); }
    const HeapVector<Value>& elements() const { return elements_; }

  private:
    HeapVector<Value> elements_;
};

struct List : public Object
{
    static GlobalRoot<Class*> ObjectClass;

    static void init();

    List(const TracedVector<Value>& values);
    List(size_t size);
    List(Traced<Class*> cls, size_t size);
    List(Traced<Class*> cls, Traced<Tuple*> init);
    List(Traced<Class*> cls, Traced<List*> init);
    void initElement(size_t index, const Value& value);

    void print(ostream& os) const override;
    void traceChildren(Tracer& t) override;

    int32_t len() { return (int32_t)elements_.size(); }
    bool contains(Traced<Value> value);
    Value getitem(size_t index) { return elements_.at(index); }
    const HeapVector<Value>& elements() const { return elements_; }

    bool setitem(Traced<Value> index, Traced<Value> value,
                 MutableTraced<Value> resultOut);
    bool delitem(Traced<Value> index, MutableTraced<Value> resultOut);
    bool append(Traced<Value> element, MutableTraced<Value> resultOut);
    void sort();

  private:
    HeapVector<Value> elements_;
};

extern void initList();

#endif
