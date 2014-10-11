#ifndef __OBJECT_H__
#define __OBJECT_H__

#include "layout.h"
#include "name.h"
#include "value.h"

#include <cassert>
#include <ostream>
#include <vector>

using namespace std;

struct Class;

struct Object
{
    static void init();

    static Class* ObjectClass;
    static Layout* InitialLayout;

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

  private:
    Class* class_;
    const Layout* layout_;
    vector<Value> slots_;

    void initAttrs(Object* base);
};

inline ostream& operator<<(ostream& s, const Object* o) {
    o->print(s);
    return s;
}

#endif
