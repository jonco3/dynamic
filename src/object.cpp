#include "object.h"

#include "bool.h"
#include "class.h"
#include "exception.h"
#include "integer.h"
#include "singletons.h"

#include "value-inl.h"

#include <cassert>
#include <iostream>
#include <memory>

GlobalRoot<Class*> Object::ObjectClass;
GlobalRoot<Layout*> Object::InitialLayout;
GlobalRoot<Object*> Object::Null;

void Object::init()
{
    InitialLayout.init(new Layout(nullptr, "__class__"));
    ObjectClass.init(new Class("Object"));
    Null.init(nullptr);
}

Object::Object(Traced<Class*> cls, Traced<Object*> base, Traced<Layout*> layout)
  : class_(cls), layout_(layout)
{
    if (cls != Class::ObjectClass)
        assert(base == cls);
    assert(layout_);
    assert(layout_->subsumes(InitialLayout));
    if (Class::ObjectClass)
        initAttrs(base);
}

Object::Object(Traced<Class*> cls, Traced<Layout*> layout)
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

void Object::initClass(Traced<Class*> cls, Traced<Object*> base)
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
    // todo: root outwards
    Root<Value> value(base);
    setAttr("__class__", value);
}

void Object::extend(Traced<Layout*> layout)
{
    assert(layout);
    assert(layout->subsumes(layout_));

    unsigned count = 0;
    for (Layout* l = layout; l != layout_; l = l->parent()) {
        assert(layout);
        ++count;
    }
    unsigned initialSize = slots_.size();
    slots_.resize(initialSize + count);
    for (unsigned i = initialSize; i < layout_->slotCount(); ++i)
        slots_[i] = UninitializedSlot;
    layout_ = layout;
}

bool Object::hasOwnAttr(Name name) const
{
    return layout_->lookupName(name) != -1;
}

bool Object::hasAttr(Name name) const
{
    if (hasOwnAttr(name))
        return true;

    // lookup attribute in class hierarchy
    Value base;
    if (!maybeGetOwnAttr("__class__", base))
        return false;

    while (Object* obj = base.toObject()) {
        if (obj->hasOwnAttr(name))
            return true;
        if (!obj->maybeGetOwnAttr("__base__", base))
            break;
    }

    return false;
}

bool Object::maybeGetOwnAttr(Name name, Value& valueOut) const
{
    int slot = layout_->lookupName(name);
    if (slot == -1)
        return false;
    return getSlot(name, slot, valueOut);
}

bool Object::maybeGetAttr(Name name, Value& valueOut) const
{
    if (maybeGetOwnAttr(name, valueOut))
        return true;

    // lookup attribute in class hierarchy
    Value base;
    if (!maybeGetOwnAttr("__class__", base))
        return false;

    while (Object* obj = base.toObject()) {
        if (obj->maybeGetOwnAttr(name, valueOut))
            return true;
        if (!obj->maybeGetOwnAttr("__base__", base))
            break;
    }

    return false;
}

bool Object::getAttrOrRaise(Name name, Value& valueOut) const
{
    bool ok = maybeGetAttr(name, valueOut);
    if (ok)
        return true;

    string message = "AttributeError: '" + class_->name() +
        "' object has no attribute '" + name + "'";
    valueOut = new Exception(message);
    return false;
}


Value Object::getAttr(Name name) const
{
    Value value = None;
    bool ok = maybeGetAttr(name, value);
    assert(ok);
    (void)ok;
    return value;
}

bool Object::getSlot(Name name, int slot, Value& valueOut) const
{
    assert(slot >= 0 && static_cast<size_t>(slot) < slots_.size());
    if (slots_[slot] == UninitializedSlot) {
        valueOut = new Exception("UnboundLocalError:: name '" +
                                 name + "' has not been bound");
        return false;
    }
    valueOut = slots_[slot];
    return true;
}

void Object::setAttr(Name name, Traced<Value> value)
{
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
    s << " { ";
    unsigned slot = slots_.size() - 1;
    Layout* layout = layout_;
    while (layout) {
        s << layout->name() << ": " << slots_[slot];
        if (slot != 0)
            s << ", ";
        layout = layout->parent();
        slot--;
    }
    s << " } ";
}

bool Object::isTrue() const
{
    // the following values are interpreted as false: False, None, numeric zero
    // of all types, and empty strings and containers (including strings,
    // tuples, lists, dictionaries, sets and frozensets).
    // https://docs.python.org/2/reference/expressions.html#boolean-operations
    // todo: call __nonzero__ and __empty__
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

#ifdef BUILD_TESTS

#include "test.h"
#include "integer.h"

testcase(object)
{
    Root<Object*> o(new Object);
    Value v;

    testFalse(o->maybeGetAttr("foo", v));
    Root<Value> one(Integer::get(1));
    o->setAttr("foo", one);
    testTrue(o->maybeGetAttr("foo", v));
    Root<Object*> obj(v.toObject());
    testTrue(obj->is<Integer>());
    testEqual(obj->as<Integer>()->value(), 1);

    testFalse(None->isTrue());
    testFalse(Integer::get(0).isTrue());
    testTrue(Integer::get(1).isTrue());
}

#endif
