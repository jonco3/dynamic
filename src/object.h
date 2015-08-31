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

typedef bool (*NativeFunc)(TracedVector<Value>, MutableTraced<Value>);

extern GlobalRoot<Object*> None;

extern void initObject();

struct Object : public Cell
{
    static GlobalRoot<Class*> ObjectClass;
    static GlobalRoot<Layout*> InitialLayout;

    static Object* create();

    Object(Traced<Class*> cls, Traced<Layout*> layout = InitialLayout);
    virtual ~Object() {}

    template <typename T> bool is() const {
        return class_ == T::ObjectClass;
    }

    template <typename T> bool isInstanceOf() const {
        return isInstanceOf(T::ObjectClass);
    }

    template <typename T> const T* as() const {
        assert(isInstanceOf(T::ObjectClass));
        return static_cast<const T*>(this);
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

    Class* type() const { return class_; }
    Layout* layout() const { return layout_; }


    // Own attribute accessors
    bool hasOwnAttr(Name name) const;
    bool maybeGetOwnAttr(Name name, MutableTraced<Value> valueOut) const;
    bool maybeDelOwnAttr(Name name);

    // Inherited attribute accessors, without descriptors
    bool hasAttr(Name name) const;
    bool maybeGetAttr(Name name, MutableTraced<Value> valueOut) const;
    Value getAttr(Name name) const;
    void initAttr(Name name, Traced<Value> value);
    void setAttr(Name name, Traced<Value> value);

    // Slot accessors
    bool hasSlot(int slot) const;
    Value getSlot(int slot) const;
    void setSlot(int slot, Traced<Value> value);

    // Add uninitialised attributes for all names in layout.
    void extend(Traced<Layout*> layout);

    bool isTrue() const;
    bool isInstanceOf(Class* cls) const;

  protected:
    Object(Traced<Class*> cls, Traced<Class*> base,
           Traced<Layout*> layout = InitialLayout);

    // Only for use during initialization
    void init(Traced<Class*> cls);

    void traceChildren(Tracer& t) override;

    bool getSlot(Name name, int slot, MutableTraced<Value> valueOut) const;

    int findOwnAttr(Name name) const;

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
          Traced<Layout*> initialLayout = InitialLayout, bool final = false);

    Object* base() const { return base_; }
    const string& name() const { return name_; }
    bool isFinal() const { return final_; }

    bool isDerivedFrom(Class* base) const;

    void traceChildren(Tracer& t) override;

    void print(ostream& s) const override;

  private:
    string name_;
    Object* base_;
    bool final_;

    // Only for use during initialization
    void init(Traced<Object*> base);

    friend void initObject();
};

extern bool object_new(TracedVector<Value> args, MutableTraced<Value> resultOut);

extern void initNativeMethod(Traced<Object*> cls, const string& name,
                             NativeFunc func,
                             unsigned minArgs, unsigned maxArgs = 0);

extern bool checkInstanceOf(Traced<Value> v, Traced<Class*> cls,
                            MutableTraced<Value> resultOut);

template <typename T>
/* static */ inline Class* Class::createNative(string name, Traced<Class*> base)
{
    Stack<Class*> cls(gc.create<Class>(name, base, InitialLayout, true));
    NativeFunc func =
        [] (TracedVector<Value> args, MutableTraced<Value> resultOut) -> bool
        {
            if (!checkInstanceOf(args[0], Class::ObjectClass, resultOut))
                return false;
            Stack<Class*> cls(args[0].asObject()->as<Class>());
            resultOut = gc.create<T>(cls);
            return true;
        };
    initNativeMethod(cls, "__new__", func, 1, 1);
    return cls;
}

// Full attribute accessors including descriptors
extern bool getAttr(Traced<Value> value, Name name,
                    MutableTraced<Value> resultOut);
extern bool getMethodAttr(Traced<Value> value, Name name,
                          MutableTraced<Value> resultOut,
                          bool& isCallableDescriptor);
extern bool setAttr(Traced<Object*> obj, Name name, Traced<Value> value,
                    MutableTraced<Value> resultOut);
extern bool delAttr(Traced<Object*> obj, Name name, MutableTraced<Value>
                    resultOut);

#endif
