#ifndef __OBJECT_H__
#define __OBJECT_H__

#include "gc.h"
#include "layout.h"
#include "name.h"
#include "value.h"

#include <cassert>
#include <ostream>
#include <vector>

using namespace std;

struct Class;

struct Object : public Cell
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;
    static GlobalRoot<Layout*> InitialLayout;
    static GlobalRoot<Object*> Null;

    Object(Traced<Class*> cls = ObjectClass, Traced<Layout*> layout = InitialLayout);
    virtual ~Object();

    // Only for use during initialization
    void initClass(Traced<Class*> cls, Traced<Object*> base);

    template <typename T> bool is() { return class_ == T::ObjectClass; }

    template <typename T> T* as() {
        assert(is<T>());
        return static_cast<T*>(this);
    }

    virtual void print(ostream& os) const;

    bool hasAttr(Name name) const;
    bool maybeGetAttr(Name name, Value& valueOut) const;
    Value getAttr(Name name) const;
    bool getAttrOrRaise(Name name, Value& valueOut) const;
    void setAttr(Name name, Traced<Value> value);

    // Add uninitialised attributes for all names in layout.
    void extend(Traced<Layout*> layout);

    Layout* layout() { return layout_; }

    bool isTrue() const;

  protected:
    Object(Traced<Class*> cls, Traced<Object*> base,
           Traced<Layout*> layout = InitialLayout);

    virtual void traceChildren(Tracer& t);
    virtual size_t size() const { return sizeof(*this); }

    bool hasOwnAttr(Name name) const;
    bool maybeGetOwnAttr(Name name, Value& valueOut) const;
    bool getSlot(Name name, int slot, Value& valueOut) const;

  private:
    Class* class_;
    Layout* layout_;
    vector<Value> slots_;

    void initAttrs(Object* base);
};

#endif
