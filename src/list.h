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
    static Tuple* get(size_t size);
    static Tuple* get(Traced<Class*> cls, size_t size);
    static Tuple* get(Traced<Class*> cls, Traced<Tuple*> init);
    static Tuple* get(Traced<Class*> cls, Traced<List*> init);
    void initElement(size_t index, const Value& value);

    void print(ostream& s) const override;
    void traceChildren(Tracer& t) override;

    int32_t len() { return size_; }
    Value getitem(size_t index) {
        assert(index < size_);
        return elements_[index];
    }


  private:
    friend struct GC;
    Tuple(const TracedVector<Value>& values);
    Tuple(size_t size);
    Tuple(Traced<Class*> cls, size_t size);
    Tuple(Traced<Class*> cls, Traced<Tuple*> init);
    Tuple(Traced<Class*> cls, Traced<List*> init);

    int32_t size_;
    Heap<Value> elements_[0];
};

struct List : public Object
{
    static GlobalRoot<Class*> ObjectClass;

    static void init();

    template <typename... Args>
    static List* get(Args&&... args);
    List(const TracedVector<Value>& values);
    List(size_t size);
    List(Traced<Class*> cls, size_t size);
    List(Traced<Class*> cls, Traced<Tuple*> init);
    List(Traced<Class*> cls, Traced<List*> init);
    void initElement(size_t index, const Value& value);

    void print(ostream& os) const override;
    void traceChildren(Tracer& t) override;

    int32_t len() const { return (int32_t)elements_.size(); }
    Value getitem(size_t index) const { return elements_.at(index); }
    const HeapVector<Value>& elements() const { return elements_; }

    void setitem(int32_t index, Value value);
    bool delitem(Traced<Value> index, MutableTraced<Value> resultOut);
    void replaceitems(int32_t start, int32_t count, Traced<List*> list);
    void append(Traced<Value> element);
    void sort();

  private:
    HeapVector<Value> elements_;
};

extern void initList();

#endif
