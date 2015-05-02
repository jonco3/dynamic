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

    Object(Traced<Class*> cls = ObjectClass,
           Traced<Layout*> layout = InitialLayout);
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

    virtual void print(ostream& os) const;

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

    // Add uninitialised attributes for all names in layout.
    void extend(Traced<Layout*> layout);

    bool isTrue() const;
    bool isInstanceOf(Class* cls) const;

  protected:
    Object(Traced<Class*> cls, Traced<Class*> base,
           Traced<Layout*> layout = InitialLayout);

    // Only for use during initialization
    void init(Traced<Class*> cls);

    virtual void traceChildren(Tracer& t) override;

    bool hasOwnAttr(Name name) const;
    bool maybeGetOwnAttr(Name name, Value& valueOut, AutoAssertNoGC& nogc) const;
    bool getSlot(Name name, int slot, Value& valueOut, AutoAssertNoGC& nogc) const;

  private:
    Layout* layout_;
    vector<Value> slots_;

    static const unsigned ClassSlot = 0;

    void initAttrs(Traced<Class*> cls);

    friend void initObject();
};

struct Class : public Object
{
    static GlobalRoot<Class*> ObjectClass;
    static GlobalRoot<Layout*> InitialLayout;

    Class(string name, Traced<Object*> base = None,
          Traced<Layout*> initialLayout = InitialLayout);

    typedef bool (*Func)(TracedVector<Value>, Root<Value>&);
    Class(string name, Traced<Object*> base,
          Func createFunc, unsigned minArgs, unsigned maxArgs = 0,
          Traced<Layout*> initialLayout = InitialLayout);

    Object* base();
    const string& name() const { return name_; }
    Native* nativeConstructor() const { return nativeConstructor_; }

    virtual void traceChildren(Tracer& t) override;
    virtual void print(ostream& s) const;

  private:
    string name_;
    Native* nativeConstructor_;

    // Only for use during initialization
    void init(Traced<Object*> base);

    friend void initObject();
};

#endif
