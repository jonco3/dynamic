#include "object.h"

#include "bool.h"
#include "callable.h"
#include "exception.h"
#include "integer.h"
#include "singletons.h"

#include "value-inl.h"

#include <cassert>
#include <iostream>
#include <memory>

struct NoneObject : public Object
{
    static GlobalRoot<Class*> ObjectClass;

    NoneObject() : Object(ObjectClass) {}
    virtual void print(ostream& s) const { s << "None"; }

    friend void initObject();
};

static const Name ClassAttr = "__class__";
static const Name BaseAttr = "__base__";
static const Name NewAttr = "__new__";

GlobalRoot<Class*> Object::ObjectClass;
GlobalRoot<Layout*> Object::InitialLayout;

GlobalRoot<Class*> Class::ObjectClass;
GlobalRoot<Layout*> Class::InitialLayout;

GlobalRoot<Layout*> NativeClass::InitialLayout;

GlobalRoot<Object*> None;
GlobalRoot<Class*> NoneObject::ObjectClass;

Object::Object(Traced<Class*> cls, Traced<Class*> base, Traced<Layout*> layout)
  : layout_(layout)
{
    if (cls != Class::ObjectClass)
        assert(base == cls);
    assert(layout_);
    assert(layout_->subsumes(InitialLayout));
    if (Class::ObjectClass)
        initAttrs(base);
}

Object::Object(Traced<Class*> cls, Traced<Layout*> layout)
  : layout_(layout)
{
    assert(layout_);
    assert(layout_->subsumes(InitialLayout));
    if (Class::ObjectClass)
        initAttrs(cls);
}

void Object::init(Traced<Class*> cls)
{
    assert(cls);
    initAttrs(cls);
}

void Object::initAttrs(Traced<Class*> cls)
{
    assert(layout_);
    slots_.resize(layout_->slotCount());
    for (unsigned i = 0; i < layout_->slotCount(); ++i)
        slots_[i] = UninitializedSlot;
    setAttr(ClassAttr, cls);
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

int Object::findOwnAttr(Name name) const
{
    return layout_->lookupName(name);
}

bool Object::hasOwnAttr(Name name) const
{
    return findOwnAttr(name) != -1;
}

bool Object::hasAttr(Name name) const
{
    if (hasOwnAttr(name))
        return true;

    AutoAssertNoGC nogc;

    // lookup attribute in class hierarchy
    Value base;
    if (!maybeGetOwnAttr(ClassAttr, base, nogc))
        return false;

    while (Object* obj = base.toObject()) {
        if (obj->hasOwnAttr(name))
            return true;
        if (!obj->maybeGetOwnAttr(BaseAttr, base, nogc))
            break;
    }

    return false;
}

int Object::findAttr(Name name, Root<Class*>& classOut) const
{
    int slot = findOwnAttr(name);
    if (slot != -1) {
        classOut = nullptr;
        return slot;
    }

    AutoAssertNoGC nogc;

    // lookup attribute in class hierarchy
    Value base;
    if (!maybeGetOwnAttr(ClassAttr, base, nogc))
        return false;

    while (Class* cls = base.toObject()->as<Class>()) {
        slot = cls->findOwnAttr(name);
        if (slot != -1) {
            classOut = cls;
            return slot;
        }
        if (!cls->maybeGetOwnAttr(BaseAttr, base, nogc))
            break;
    }

    classOut = nullptr;
    return -1;
}

bool Object::maybeGetOwnAttr(Name name, Value& valueOut, AutoAssertNoGC& nogc) const
{
    int slot = layout_->lookupName(name);
    if (slot == -1)
        return false;
    return getSlot(name, slot, valueOut, nogc);
}

bool Object::maybeGetAttr(Name name, Root<Value>& valueOut) const
{
    AutoAssertNoGC nogc;

    if (maybeGetOwnAttr(name, valueOut, nogc))
        return true;

    // lookup attribute in class hierarchy
    Value base;
    if (!maybeGetOwnAttr(ClassAttr, base, nogc))
        return false;

    while (Object* obj = base.toObject()) {
        if (obj->maybeGetOwnAttr(name, valueOut, nogc))
            return true;
        if (!obj->maybeGetOwnAttr(BaseAttr, base, nogc))
            break;
    }

    return false;
}

Value Object::getAttr(Name name) const
{
    Root<Value> value;
    bool ok = maybeGetAttr(name, value);
    assert(ok);
    (void)ok;
    return value;
}

Value Object::getOwnAttr(Name name) const
{
    AutoAssertNoGC nogc;
    Root<Value> value;
    bool ok = maybeGetOwnAttr(name, value, nogc);
    assert(ok);
    (void)ok;
    return value;
}

bool Object::getSlot(Name name, int slot, Value& valueOut, AutoAssertNoGC& nogc) const
{
    assert(slot >= 0 && static_cast<size_t>(slot) < slots_.size());
    if (slots_[slot] == Value(UninitializedSlot)) {
        valueOut = gc::create<Exception>("UnboundLocalError",
                                 "name '" + name + "' has not been bound");
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
    s << type()->name() << "@" << hex << reinterpret_cast<uintptr_t>(this);
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

Class* Object::type() const
{
    return getAttr(ClassAttr).asObject()->as<Class>();
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

bool Object::isInstanceOf(Traced<Class*> cls) const
{
    Object* obj = getOwnAttr("__class__").toObject();
    do {
        if (obj == cls)
            return true;
        obj = obj->getOwnAttr("__base__").toObject();
    } while (obj != None);
    return false;
}

void Object::traceChildren(Tracer& t)
{
    gc::trace(t, &layout_);
    for (auto i = slots_.begin(); i != slots_.end(); ++i)
        gc::trace(t, &*i);
}

Class::Class(string name, Traced<Object*> base, Traced<Layout*> initialLayout) :
  Object(ObjectClass, initialLayout), name_(name)
{
    assert(base == None || base->is<Class>());
    assert(initialLayout->subsumes(InitialLayout));
    if (Class::ObjectClass)
        setAttr(BaseAttr, base);
}

void Class::init(Traced<Object*> base)
{
    Object::init(ObjectClass);
    setAttr(BaseAttr, base);
}

void Class::print(ostream& s) const
{
    s << "Class " << name_;
}

NativeClass::NativeClass(string name, Func createFunc,
                         unsigned minArgs, unsigned maxArgs,
                         Traced<Layout*> initialLayout)
  : Class(name, None, initialLayout)
{
    assert(minArgs >= 1); // Class is passed as first argument.
    assert(initialLayout->subsumes(InitialLayout));
    Root<NativeClass*> self(this);
    initNativeMethod(self, "__new__", createFunc, minArgs, maxArgs);
}

void initObject()
{
    Object::InitialLayout.init(gc::create<Layout>(nullptr, ClassAttr));
    Class::InitialLayout.init(
        gc::create<Layout>(Object::InitialLayout, BaseAttr));
    NativeClass::InitialLayout.init(
        gc::create<Layout>(Class::InitialLayout, NewAttr));

    assert(Object::InitialLayout->lookupName(ClassAttr) == Object::ClassSlot);

    Object::ObjectClass.init(gc::create<Class>("Object"));
    NoneObject::ObjectClass.init(gc::create<Class>("None"));
    Class::ObjectClass.init(gc::create<Class>("Class"));

    None.init(gc::create<NoneObject>());

    Class::ObjectClass->init(Object::ObjectClass);
    Object::ObjectClass->init(None);
    NoneObject::ObjectClass->init(Class::ObjectClass);
}

#ifdef BUILD_TESTS

#include "test.h"
#include "integer.h"

testcase(object)
{
    Root<Object*> o(gc::create<Object>());

    Root<Value> v;
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
