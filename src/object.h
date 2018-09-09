#ifndef __OBJECT_H__
#define __OBJECT_H__

#include "assert.h"
#include "gcdefs.h"
#include "layout.h"
#include "name.h"
#include "vector.h"
#include "value.h"

#include <ostream>

using namespace std;

struct Class;
struct Native;
struct Tuple;

using NativeArgs = const TracedVector<Value>&;
using NativeFunc = bool (*)(NativeArgs, MutableTraced<Value>);

extern GlobalRoot<Object*> None;

extern void initObject();
extern void initObject2();

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
    int findOwnAttr(Name name) const;
    Value getSlot(int slot) const;
    void setSlot(int slot, Value value);

    // Add uninitialised attributes for all names in layout.
    void extend(Traced<Layout*> layout);

    static bool IsTrue(Traced<Object*> obj);
    bool isInstanceOf(Class* cls) const;

  protected:
    template <size_t N>
    Object(uint8_t (&inlineData)[N], Traced<Class*> cls, Traced<Layout*> layout)
      : class_(nullptr),
        layout_(layout)
    {
        slots_.initInlineData(inlineData);
        init(cls, layout);
    }

    // Only for use during initialization
    void finishInit(Traced<Class*> cls);

    void traceChildren(Tracer& t) override;

    bool getSlot(Name name, int slot, MutableTraced<Value> valueOut) const;

    bool hasOutOfLineSlots() const {
        return slots_.hasHeapElements();
    }

  private:
    Heap<Class*> class_;
    Heap<Layout*> layout_;
    HeapVector<Value, InlineVectorBase<Value>> slots_;

    void init(Traced<Class*> cls, Traced<Layout*> layout);

    friend void initObject();
};

template <size_t N>
struct InlineObject : public Object
{
    InlineObject(Traced<Class*> cls, Traced<Layout*> layout = InitialLayout)
      : Object(inlineData_, cls, layout)
    {}

  private:
    alignas(Value) uint8_t inlineData_[N * sizeof(Value)];
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

    Object* base() const; // todo: to be removed.
    Tuple* bases() const { return bases_; }
    Tuple* mro();
    const string& name() const { return name_; }
    bool isFinal() const { return final_; }

    bool isDerivedFrom(Class* base) const;

    void traceChildren(Tracer& t) override;

    void print(ostream& s) const override;

  private:
    string name_;
    Heap<Tuple*> bases_;
    Heap<Tuple*> mro_;
    bool final_;

    // Only for use during initialization
    void finishInit(Traced<Class*> base);
    void finishInitNoBases();

    void initBases(Traced<Class*> base);
    void initMRO(Traced<Class*> base);

    friend void initObject();
    friend void initObject2();
};

extern bool object_new(NativeArgs args, MutableTraced<Value> resultOut);

extern void initAttr(Traced<Object*> cls, const string& name,
                     Traced<Value> value);

extern void initNativeMethod(Traced<Object*> obj, const string& name,
                             NativeFunc func,
                             unsigned minArgs, unsigned maxArgs = 0);

extern bool checkInstanceOf(Traced<Value> v, Traced<Class*> cls,
                            MutableTraced<Value> resultOut);

template <typename T>
/* static */ inline Class* Class::createNative(string name, Traced<Class*> base)
{
    Stack<Class*> cls(gc.create<Class>(name, base, InitialLayout, true));
    NativeFunc func =
        [] (NativeArgs args, MutableTraced<Value> resultOut) -> bool
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

// Abstraction for the result of looking up a method attribute.
//
// If the attribute is a descriptor but is a Callable (i.e. a Fuction or a
// Native), then don't call the descriptor's __get__ method which will create a
// new Method object.  Instead return the Callable as-is and set the isCallable
// flag.  We use this to skip the unnecessary object creation.
struct StackMethodAttr
{
    Stack<Value> method;
    bool isCallable;

    unsigned extraArgs() const {
        return isCallable ? 1 : 0;
    }
};

// Full attribute accessors including descriptors
extern bool getAttr(Traced<Value> value, Name name,
                    MutableTraced<Value> resultOut);
extern bool getMethodAttr(Traced<Value> value, Name name,
                          StackMethodAttr& resultOut);
extern bool getSpecialMethodAttr(Traced<Value> value, Name name,
                                 StackMethodAttr& resultOut);
extern bool setAttr(Traced<Object*> obj, Name name, Traced<Value> value,
                    MutableTraced<Value> resultOut);
extern bool delAttr(Traced<Object*> obj, Name name, MutableTraced<Value>
                    resultOut);

template <typename W, typename T>
inline WrapperMixins<W, T*>::operator Traced<Value> () const
{
    static_assert(is_base_of<Object, T>::value,
                  "T must derive from object for this conversion");
    // Since Traced<T> is immutable and all Objects are Values, we can
    // safely cast a Stack<Object*> to a Traced<Value>.
    const Value* ptr = reinterpret_cast<Value const*>(extract());
    return Traced<Value>::fromTracedLocation(ptr);
}

template <typename W, typename T>
template <typename S>
inline WrapperMixins<W, T*>::operator Traced<S*> () const
{
    static_assert(is_base_of<S, T>::value,
                  "T must derive from S for this conversion");
    S* const * ptr = reinterpret_cast<S* const *>(extract());
    return Traced<S*>::fromTracedLocation(ptr);
}

#endif
