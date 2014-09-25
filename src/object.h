#ifndef __OBJECT_H__
#define __OBJECT_H__

#include "value.h"
#include "name.h"
#include "layout.h"

#include <vector>
#include <ostream>
#include <cassert>

using namespace std;

struct Class;

struct Object
{
    Object(Class *cls);
    Object(Class *cls, const Layout *layout);
    virtual ~Object();

    template <typename T> bool is() { return cls == &T::Class; }

    template <typename T> T* as() {
        assert(is<T>());
        return static_cast<T*>(this);
    }

    virtual void print(ostream& os) const;

    bool getProp(Name name, Value& valueOut) const;
    void setProp(Name name, Value value);

    const Layout* getLayout() const { return layout; }

  private:
    Class* cls;
    const Layout* layout;
    vector<Value> slots;

    void initAttrs();
};

inline ostream& operator<<(ostream& s, const Object* o) {
    o->print(s);
    return s;
}

#endif
