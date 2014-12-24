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
    int findOwnAttr(Name name) const;

    bool maybeGetAttr(Name name, Value& valueOut) const;
    Value getAttr(Name name) const;
    bool raiseAttrError(Name name, Value& valueOut) const;
    bool getAttrOrRaise(Name name, Value& valueOut) const;

    // Get the slot of and optionally the class containing the named attribute
    int findAttr(Name name, Root<Class*>& classOut) const;

    void setAttr(Name name, Traced<Value> value);

    // Add uninitialised attributes for all names in layout.
    void extend(Traced<Layout*> layout);

    Layout* layout() { return layout_; }

    bool isTrue() const;

    virtual bool equals(Object* other) const { return this == other; }
    virtual size_t hash() const { return size_t(this); }

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
