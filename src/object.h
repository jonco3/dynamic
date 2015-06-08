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

typedef bool (*NativeFunc)(TracedVector<Value>, Root<Value>&);

extern GlobalRoot<Object*> None;

extern void initObject();

struct Object : public Cell
{
    static GlobalRoot<Class*> ObjectClass;
    static GlobalRoot<Layout*> InitialLayout;

    static Object* create();

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

    // Attribute accessors
    bool hasAttr(Name name) const;
    bool maybeGetAttr(Name name, Root<Value>& valueOut) const;
    Value getAttr(Name name) const;
    void initAttr(Name name, Traced<Value> value);
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

    bool getSlot(Name name, int slot, Root<Value>& valueOut) const;
    Value getSlot(int slot) const;
    void setSlot(int slot, Traced<Value> value);

    int findOwnAttr(Name name) const;
    bool hasOwnAttr(Name name) const;
    bool maybeGetOwnAttr(Name name, Root<Value>& valueOut) const;

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

    // Create a python class corresponding to a native (C++) class.
    // The class must provides a constructor that takes a single Traced<Class*>
    // argument.
    template <typename T>
    static inline Class* createNative(string name,
                                      Traced<Class*> base = Object::ObjectClass);

    // Create a python class corresponding to a native (C++) class specifying a
    // function to create object instances.
    static Class* createNative(string name,
                               NativeFunc newFunc, unsigned maxArgs = 1,
                               Traced<Class*> base = Object::ObjectClass);

    // Be careful using this because instances created from python will be
    // allocated as Object.
    Class(string name, Traced<Class*> base = Object::ObjectClass,
          Traced<Layout*> initialLayout = InitialLayout);

    Object* base();
    const string& name() const { return name_; }

    bool isDerivedFrom(Class* base) const;

    void print(ostream& s) const override;

  private:
    string name_;

    // Only for use during initialization
    void init(Traced<Object*> base);

    friend void initObject();
};

extern bool object_new(TracedVector<Value> args, Root<Value>& resultOut);

extern void initNativeMethod(Traced<Object*> cls, Name name, NativeFunc func,
                             unsigned minArgs, unsigned maxArgs = 0);

extern bool checkInstanceOf(Traced<Value> v, Traced<Class*> cls,
                            Root<Value>& resultOut);

template <typename T>
/* static */ inline Class* Class::createNative(string name, Traced<Class*> base)
{
    Root<Class*> cls(gc.create<Class>(name, base));
    NativeFunc func =
        [] (TracedVector<Value> args, Root<Value>& resultOut) -> bool
        {
            if (!checkInstanceOf(args[0], Class::ObjectClass, resultOut))
                return false;
            Root<Class*> cls(args[0].asObject()->as<Class>());
            resultOut = gc.create<T>(cls);
            return true;
        };
    initNativeMethod(cls, "__new__", func, 1, 1);
    return cls;
}

#endif
