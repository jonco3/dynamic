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
struct Object;

extern GlobalRoot<Object*> None;

extern void initObject();

struct Object : public Cell
{
    static GlobalRoot<Class*> ObjectClass;
    static GlobalRoot<Layout*> InitialLayout;

    Object(Traced<Class*> cls = ObjectClass,
           Traced<Layout*> layout = InitialLayout);
    virtual ~Object() {}

    template <typename T> bool is() { return class_ == T::ObjectClass; }

    template <typename T> T* as() {
        assert(is<T>());
        return static_cast<T*>(this);
    }

    bool isNone() {
        return this == None;
    }

    virtual void print(ostream& os) const;

    const Class* getClass() const { return class_; }
    Layout* layout() { return layout_; }
    Class* getType() const;

    bool hasAttr(Name name) const;
    int findOwnAttr(Name name) const;

    bool maybeGetAttr(Name name, Root<Value>& valueOut) const;
    Value getOwnAttr(Name name) const;
    Value getAttr(Name name) const;

    // Get the slot of and optionally the class containing the named attribute
    int findAttr(Name name, Root<Class*>& classOut) const;

    void setAttr(Name name, Traced<Value> value);

    // Add uninitialised attributes for all names in layout.
    void extend(Traced<Layout*> layout);

    bool isTrue() const;
    bool isInstanceOf(Traced<Class*> cls);

    virtual bool equals(Object* other) const { return this == other; }
    virtual size_t hash() const { return size_t(this); }

  protected:
    Object(Traced<Class*> cls, Traced<Class*> base,
           Traced<Layout*> layout = InitialLayout);

    // Only for use during initialization
    void init(Traced<Class*> cls);

    virtual void traceChildren(Tracer& t);
    virtual size_t size() const { return sizeof(*this); }

    bool hasOwnAttr(Name name) const;
    bool maybeGetOwnAttr(Name name, Value& valueOut, AutoAssertNoGC& nogc) const;
    bool getSlot(Name name, int slot, Value& valueOut, AutoAssertNoGC& nogc) const;

  private:
    Class* class_;
    Layout* layout_;
    vector<Value> slots_;

    void initAttrs(Traced<Class*> cls);

    friend void initObject();
};

struct Class : public Object
{
    static GlobalRoot<Class*> ObjectClass;
    static GlobalRoot<Layout*> InitialLayout;

    Class(string name, Traced<Object*> base = None,
          Traced<Layout*> initialLayout = InitialLayout);

    const string& name() const { return name_; }

    virtual void print(ostream& s) const;

  private:
    string name_;

    // Only for use during initialization
    void init(Traced<Object*> base);

    friend void initObject();
};

#endif
