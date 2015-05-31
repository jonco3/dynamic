#ifndef __OBJECT_H__
#define __OBJECT_H__

#include "gcdefs.h"
#include "value.h"
#include "layout.h"
#include "name.h"

#include <cassert>
#include <ostream>
#include <vector>

using namespace std;

struct Class;
struct Native;
struct Object;

extern GlobalRoot<Object*> None;

extern void initObject();

struct Object : public Cell
{
    static GlobalRoot<Class*> ObjectClass;
    static GlobalRoot<Layout*> InitialLayout;

    static Object* create();
    static Object* createInstance(Traced<Class*> cls);

    Object(Traced<Class*> cls, Traced<Layout*> layout = InitialLayout);
    virtual ~Object() {}

    template <typename T> bool is() {
        return slots_[ClassSlot].asObject() == T::ObjectClass;
    }

    template <typename T> T* as() {
        assert(isInstanceOf(T::ObjectClass));
        return static_cast<T*>(this);
    }

    bool isNone() {
        return this == None;
    }

    void print(ostream& s) const override;
    virtual void dump(ostream& s) const;

    Layout* layout() const { return layout_; }
    Class* type() const;

    bool hasAttr(Name name) const;
    int findOwnAttr(Name name) const;

    bool maybeGetAttr(Name name, Root<Value>& valueOut) const;
    Value getOwnAttr(Name name) const;
    Value getAttr(Name name) const;

    // Get the slot of and optionally the class containing the named attribute
    int findAttr(Name name, Root<Class*>& classOut) const;

    void setAttr(Name name, Traced<Value> value);

    bool maybeDelOwnAttr(Name name);

    // Add uninitialised attributes for all names in layout.
    void extend(Traced<Layout*> layout);

    bool isTrue() const;
    bool isInstanceOf(Class* cls) const;

  protected:
    enum {
        ClassSlot,
        SlotCount
    };

    Object(Traced<Class*> cls, Traced<Class*> base,
           Traced<Layout*> layout = InitialLayout);

    // Only for use during initialization
    void init(Traced<Class*> cls);

    virtual void traceChildren(Tracer& t) override;

    bool hasOwnAttr(Name name) const;
    bool maybeGetOwnAttr(Name name, Root<Value>& valueOut) const;
    bool getSlot(Name name, int slot, Root<Value>& valueOut) const;

    Value getSlot(int slot) const;
    void setSlot(int slot, Traced<Value> value);

  private:
    Layout* layout_;
    vector<Value> slots_;

    void initAttrs(Traced<Class*> cls);

    friend void initObject();
};

struct Class : public Object
{
    static GlobalRoot<Class*> ObjectClass;
    static GlobalRoot<Layout*> InitialLayout;

    Class(string name, Traced<Class*> base = Object::ObjectClass,
          Traced<Layout*> initialLayout = InitialLayout);

    typedef Object* (*NativeConstructor)(Traced<Class*> cls);
    Class(string name, Traced<Class*> maybeBase,
          NativeConstructor constructor,
          Traced<Layout*> initialLayout = InitialLayout);

    Object* base();
    const string& name() const { return name_; }
    NativeConstructor nativeConstructor() const { return constructor_; }

    bool isDerivedFrom(Class* base) const;

    void print(ostream& s) const override;

  private:
    string name_;
    NativeConstructor constructor_;

    // Only for use during initialization
    void init(Traced<Object*> base);

    friend void initObject();
};

#endif
