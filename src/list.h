#ifndef __LIST_H__
#define __LIST_H__

#include "object.h"

struct ListIter;

struct ListBase : public Object
{
    bool len(MutableTraced<Value> resultOut);
    bool getitem(Traced<Value> index, MutableTraced<Value> resultOut);
    bool contains(Traced<Value> element, MutableTraced<Value> resultOut);
    bool iter(MutableTraced<Value> resultOut);

    virtual const string& listName() const = 0;
    void traceChildren(Tracer& t) override;

    int32_t len() { return (int32_t)elements_.size(); }
    Value getitem(size_t index) {
        return elements_.at(index);
    }

    bool operator==(const ListBase& other);
    bool operator!=(const ListBase& other) { return !(*this == other); }

    const HeapVector<Value>& elements() const { return elements_; }

  protected:
    HeapVector<Value> elements_;
    friend struct ListIter;

    ListBase(Traced<Class*> cls, const TracedVector<Value>& values);
    ListBase(Traced<Class*> cls, size_t size);

    virtual ListBase* createDerived(size_t size) = 0;

    bool checkIndex(int32_t index, MutableTraced<Value> resultOut);
};

struct Tuple : public ListBase
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    static GlobalRoot<Tuple*> Empty;

    static Tuple* get(const TracedVector<Value>& values);

    static Tuple* createUninitialised(size_t size);
    void initElement(size_t index, const Value& value);

    const string& listName() const override;
    void print(ostream& s) const override;

    Tuple(const TracedVector<Value>& values);
    Tuple(size_t size);
    Tuple(Traced<Class*> cls, Traced<ListBase*> init);

  private:
    ListBase* createDerived(size_t size) override {
        return gc.create<Tuple>(size);
    }
};

struct List : public ListBase
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    List(const TracedVector<Value>& values);
    List(size_t size);
    List(Traced<Class*> cls, Traced<ListBase*> init);

    const string& listName() const override;
    void print(ostream& os) const override;

    bool setitem(Traced<Value> index, Traced<Value> value, MutableTraced<Value> resultOut);
    bool delitem(Traced<Value> index, MutableTraced<Value> resultOut);
    bool append(Traced<Value> element, MutableTraced<Value> resultOut);

  private:

    ListBase* createDerived(size_t size) override {
        return gc.create<List>(size);
    }
};

struct Slice : public Object
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;
    static GlobalRoot<Layout*> InitialLayout;

    Slice(TracedVector<Value> args);
    Slice(Traced<Class*> cls);

    void indices(int32_t length,
                 int32_t& startOut, int32_t& stopOut, int32_t& stepOut);

  private:
    enum {
        StartSlot,
        StopSlot,
        StepSlot
    };

    int32_t getSlotOrDefault(unsigned slot, int32_t def);
};

extern void initList();

#endif
