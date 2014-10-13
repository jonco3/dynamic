#include "object.h"

#include "bool.h"
#include "class.h"
#include "integer.h"
#include "none.h"

#include "value-inl.h"

#include <cassert>
#include <iostream>
#include <memory>

GlobalRoot<Class*> Object::ObjectClass;
GlobalRoot<Layout*> Object::InitialLayout;

void Object::init()
{
    InitialLayout.init(new Layout(nullptr, "__class__"));
    ObjectClass.init(new Class("Object"));
}

Object::Object(Class *cls, Object* base, Layout* layout)
  : class_(cls), layout_(layout)
{
    if (cls != Class::ObjectClass)
        assert(base == cls);
    assert(layout_);
    assert(layout_->subsumes(InitialLayout));
    if (Class::ObjectClass)
        initAttrs(base);
}

Object::Object(Class *cls, Layout* layout)
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

bool Object::hasProp(Name name) const
{
    return layout_->lookupName(name) != -1;
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
    assert(slot >= 0 && static_cast<size_t>(slot) < slots_.size());
    if (slots_[slot] == UninitializedSlot) {
        // todo: raise exception here
        cerr << "Reference to uninitialized attribute '" << name << "'" << endl;
        return false;
    }
    valueOut = slots_[slot];
    return true;
}

void Object::setProp(Name name, Value valueArg)
{
    Root<Value> value(valueArg);
    int slot = layout_->lookupName(name);
    if (slot == -1) {
        layout_ = layout_->addName(name);
        slot = slots_.size();
        assert(layout_->lookupName(name) == slot);
        slots_.resize(slots_.size() + 1);
    }
    assert(slot >= 0 && static_cast<size_t>(slot) < slots_.size());
    slots_[slot] = value;
}

void Object::print(ostream& s) const
{
    s << class_->name() << "@" << hex << reinterpret_cast<uintptr_t>(this);
}

bool Object::isTrue() const
{
    // the following values are interpreted as false: False, None, numeric zero
    // of all types, and empty strings and containers (including strings,
    // tuples, lists, dictionaries, sets and frozensets).
    // https://docs.python.org/2/reference/expressions.html#boolean-operations

    return
        this != None &&
        this != Boolean::False &&
        this != Integer::Zero;
}

void Object::traceChildren(Tracer& t)
{
    gc::trace(t, &layout_);
    for (auto i = slots_.begin(); i != slots_.end(); ++i)
        gc::trace(t, &*i);
}


#include "test.h"
#include "integer.h"

testcase(object)
{
    Root<Object*> o(new Object);
    Value v;

    testFalse(o->getProp("foo", v));
    o->setProp("foo", Integer::get(1));
    testTrue(o->getProp("foo", v));

    testFalse(None->isTrue());
    testFalse(Integer::get(0).isTrue());
    testTrue(Integer::get(1).isTrue());
}
