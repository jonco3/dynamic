#ifndef __SET_H__
#define __SET_H__

#include "object.h"

#include <unordered_set>

struct Set : public Object
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    Set();
    Set(const TracedVector<Value>& values);
    Set(const Traced<Class*> cls);

    void traceChildren(Tracer& t) override;
    void print(ostream& s) const override;

    size_t len() const {
        return elements_.size();
    }

    bool contains(Traced<Value> element) const {
        return elements_.find(element) != elements_.end();
    }

    Value keys() const;

    void add(Traced<Value> element);
    void clear();

  private:
    using SetImpl = unordered_set<Value, ValueHash, ValuesEqual>;
    SetImpl elements_;
};

#endif
