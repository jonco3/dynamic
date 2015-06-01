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

    int32_t len() { return (int32_t)elements_.size(); }
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

    virtual ListBase* createDerived(size_t size) = 0;

    int32_t wrapIndex(int32_t index);
    bool checkIndex(int32_t index, Root<Value>& resultOut);
    int32_t clampIndex(int32_t index, bool inclusive);
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

    virtual const string& listName() const;
    virtual void print(ostream& os) const;

    bool setitem(Traced<Value> index, Traced<Value> value, Root<Value>& resultOut);
    bool delitem(Traced<Value> index, Root<Value>& resultOut);
    bool append(Traced<Value> element, Root<Value>& resultOut);

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

    int32_t start() { return getSlot(StartSlot).asInt32(); }
    int32_t stop() { return getSlot(StopSlot).asInt32(); }
    int32_t step() { return getSlot(StepSlot).asInt32(); }

  private:
    enum {
        StartSlot = Object::SlotCount,
        StopSlot,
        StepSlot
    };
};

extern void initList();

#endif
