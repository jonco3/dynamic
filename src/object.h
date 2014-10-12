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

    static GlobalRoot<Class> ObjectClass;
    static GlobalRoot<Layout> InitialLayout;

    Object(Class *cls = ObjectClass, const Layout *layout = InitialLayout);
    virtual ~Object();

    void initClass(Class* cls, Object* base); // Only for use during initialization

    template <typename T> bool is() { return class_ == T::ObjectClass; }

    template <typename T> T* as() {
        assert(is<T>());
        return static_cast<T*>(this);
    }

    virtual void print(ostream& os) const;

    bool hasProp(Name name) const;
    bool getProp(Name name, Value& valueOut) const;
    void setProp(Name name, Value value);

    const Layout* layout() const { return layout_; }

    bool isTrue() const;

  protected:
    Object(Class *cls, Object *base, const Layout* layout = InitialLayout);

    virtual void traceChildren(Tracer& t) const;
    virtual size_t size() const { return sizeof(*this); }

  private:
    Class* class_;
    const Layout* layout_;
    vector<Value> slots_;

    void initAttrs(Object* base);
};

#endif
