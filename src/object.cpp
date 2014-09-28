#include "object.h"

#include "class.h"
#include "none.h"

#include <cassert>
#include <iostream>

Class* Object::ObjectClass = nullptr;
Layout* Object::InitialLayout = nullptr;

void Object::init()
{
    InitialLayout = new Layout(nullptr, "__class__");
    ObjectClass = new Class("Object");
}

Object::Object(Class *cls, Object* base, const Layout* layout)
  : class_(cls), layout_(layout)
{
    if (cls != Class::ObjectClass)
        assert(base == cls);
    assert(layout_);
    assert(layout_->subsumes(InitialLayout));
    if (Class::ObjectClass)
        initAttrs(base);
}

Object::Object(Class *cls, const Layout* layout)
  : class_(cls), layout_(layout)
{
    assert(layout_);
    assert(layout_->subsumes(InitialLayout));
    if (Class::ObjectClass)
        initAttrs(cls);
}

Object::~Object()
{
}

void Object::initClass(Class* cls, Object* base)
{
    assert(cls);
    assert(!class_);
    class_ = cls;
    initAttrs(base);
}

void Object::initAttrs(Object* base)
{
    assert(class_);
    assert(layout_);
    slots_.resize(layout_->slotCount());
    for (unsigned i = 0; i < layout_->slotCount(); ++i)
        slots_[i] = UninitializedSlot;
    setProp("__class__", base);
}

bool Object::getProp(Name name, Value& valueOut) const
{
    assert(class_);
    int slot = layout_->lookupName(name);
    if (slot == -1) {
        // lookup attribute in class hierarchy
        const Object *o = this;
        Value cv;
        while (o->getProp("__class__", cv)) {
            o = cv.toObject();
            if (!o)
                break;
            if (o->getProp(name, valueOut))
                return true;
        }

        // todo: raise exception here
        cerr << "Object " << this << " has no attribute '" << name << "'" << endl;
        return false;
    }
    assert(slot >= 0 && slot < slots_.size());
    valueOut = slots_[slot];
    if (valueOut == UninitializedSlot) {
        // todo: raise exception here
        cerr << "Reference to uninitialized attribute '" << name << "'" << endl;
        return false;
    }
    return true;
}

void Object::setProp(Name name, Value value)
{
    int slot = layout_->lookupName(name);
    if (slot == -1) {
        layout_ = layout_->addName(name);
        slot = slots_.size();
        assert(layout_->lookupName(name) == slot);
        slots_.resize(slots_.size() + 1);
    }
    assert(slot >= 0 && slot < slots_.size());
    slots_[slot] = value;
}

void Object::print(ostream& s) const {
    s << class_->name() << "@" << reinterpret_cast<uintptr_t>(this);
}

#include "test.h"
#include "integer.h"

testcase(object)
{
    unique_ptr<Object> o(new Object);
    Value v;

    testFalse(o->getProp("foo", v));
    o->setProp("foo", Integer::get(1));
    testTrue(o->getProp("foo", v));
}
