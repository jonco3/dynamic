#include "object.h"

#include "bool.h"
#include "callable.h"
#include "exception.h"
#include "numeric.h"
#include "singletons.h"

#include "value-inl.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <sstream>

struct NoneObject : public Object
{
    static GlobalRoot<Class*> ObjectClass;

    NoneObject() : Object(ObjectClass) {}

    void print(ostream& s) const override {
        s << "None";
    }

    friend void initObject();
};

static const Name ClassAttr = "__class__";
static const Name BaseAttr = "__base__";
static const Name NewAttr = "__new__";

GlobalRoot<Class*> Object::ObjectClass;
GlobalRoot<Layout*> Object::InitialLayout;

GlobalRoot<Class*> Class::ObjectClass;
GlobalRoot<Layout*> Class::InitialLayout;

GlobalRoot<Object*> None;
GlobalRoot<Class*> NoneObject::ObjectClass;

Object* Object::create()
{
    return gc.create<Object>(ObjectClass);
}

Object* Object::createInstance(Traced<Class*> cls)
{
    return gc.create<Object>(cls);
}

Object::Object(Traced<Class*> cls, Traced<Class*> base, Traced<Layout*> layout)
  : layout_(layout)
{
    if (cls != Class::ObjectClass)
        assert(base == cls);
    assert(layout_);
    assert(layout_->subsumes(InitialLayout));
    if (Class::ObjectClass)
        initAttrs(cls);
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
    return findOwnAttr(name) != Layout::NotFound;
}

bool Object::hasAttr(Name name) const
{
    if (hasOwnAttr(name))
        return true;

    // lookup attribute in class hierarchy
    Root<Value> base;
    if (!maybeGetOwnAttr(ClassAttr, base))
        return false;

    AutoAssertNoGC nogc;
    while (Object* obj = base.toObject()) {
        if (obj->hasOwnAttr(name))
            return true;
        if (!obj->maybeGetOwnAttr(BaseAttr, base))
            break;
    }

    return false;
}

int Object::findAttr(Name name, Root<Class*>& classOut) const
{
    int slot = findOwnAttr(name);
    if (slot != Layout::NotFound) {
        classOut = nullptr;
        return slot;
    }

    // lookup attribute in class hierarchy
    Root<Value> base;
    if (!maybeGetOwnAttr(ClassAttr, base))
        return false;

    AutoAssertNoGC nogc;

    while (Class* cls = base.toObject()->as<Class>()) {
        slot = cls->findOwnAttr(name);
        if (slot != Layout::NotFound) {
            classOut = cls;
            return slot;
        }
        if (!cls->maybeGetOwnAttr(BaseAttr, base))
            break;
    }

    classOut = nullptr;
    return -1;
}

bool Object::maybeGetOwnAttr(Name name, Root<Value>& valueOut) const
{
    int slot = layout_->lookupName(name);
    if (slot == Layout::NotFound)
        return false;
    return getSlot(name, slot, valueOut);
}

bool Object::maybeGetAttr(Name name, Root<Value>& valueOut) const
{
    if (maybeGetOwnAttr(name, valueOut))
        return true;

    // lookup attribute in class hierarchy
    Root<Value> base;
    if (!maybeGetOwnAttr(ClassAttr, base))
        return false;

    while (Object* obj = base.toObject()) {
        if (obj->maybeGetOwnAttr(name, valueOut))
            return true;
        if (!obj->maybeGetOwnAttr(BaseAttr, base))
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
    Root<Value> value;
    bool ok = maybeGetOwnAttr(name, value);
    assert(ok);
    (void)ok;
    return value;
}

bool Object::getSlot(Name name, int slot, Root<Value>& valueOut) const
{
    assert(slot >= 0 && static_cast<size_t>(slot) < slots_.size());
    if (slots_[slot] == Value(UninitializedSlot)) {
        valueOut = gc.create<UnboundLocalError>(
            "name '" + name + "' has not been bound");
        return false;
    }
    valueOut = slots_[slot];
    return true;
}

Value Object::getSlot(int slot) const
{
    assert(slot >= 0 && static_cast<size_t>(slot) < slots_.size());
    assert(slots_[slot] != Value(UninitializedSlot));
    return slots_[slot];
}

void Object::setSlot(int slot, Traced<Value> value)
{
    assert(slot >= 0 && static_cast<size_t>(slot) < slots_.size());
    slots_[slot] = value;
}

void Object::setAttr(Name name, Traced<Value> value)
{
    if (value.isObject()) {
        assert(value.asObject());
        assert(value.asObject() != UninitializedSlot);
    }

    int slot = layout_->lookupName(name);
    if (slot == Layout::NotFound) {
        layout_ = layout_->addName(name);
        slot = slots_.size();
        assert(layout_->lookupName(name) == slot);
        slots_.resize(slots_.size() + 1);
    }
    assert(slot >= 0 && static_cast<size_t>(slot) < slots_.size());
    slots_[slot] = value;
}

bool Object::maybeDelOwnAttr(Name name)
{
    Layout* layout = layout_->findAncestor(name);
    if (!layout)
        return false;

    if (layout == layout_) {
        // Deleting the last slot is simple.
        assert(layout->slotCount() == slots_.size());
        slots_.resize(slots_.size() - 1);
        layout_ = layout_->parent();
        return true;
    }

    int count = layout_->slotCount() - layout->slotCount();
    assert(count >= 0);
    vector<Name> names;
    names.reserve(count);
    while (layout_ != layout) {
        names.push_back(layout_->name());
        layout_ = layout_->parent();
    }
    slots_.erase(slots_.begin() + layout_->slotIndex());
    layout_ = layout_->parent();
    for (int i = names.size() - 1; i >= 0; i--)
        layout_ = layout_->addName(names[i]);
    return true;
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
    // Cast because calling as() can end up recursively calling this.
    return static_cast<Class*>(getAttr(ClassAttr).asObject());
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

bool Object::isInstanceOf(Class* cls) const
{
    AutoAssertNoGC nogc;
    return type()->isDerivedFrom(cls);
}

void Object::traceChildren(Tracer& t)
{
    gc.trace(t, &layout_);
    for (auto i = slots_.begin(); i != slots_.end(); ++i)
        gc.trace(t, &*i);
}

Class::Class(string name, Traced<Class*> base, Traced<Layout*> initialLayout) :
  Object(ObjectClass, initialLayout), name_(name)
{
    assert(base);
    assert(initialLayout->subsumes(InitialLayout));
    if (Class::ObjectClass)
        setAttr(BaseAttr, base);
    constructor_ = base->nativeConstructor();
}

Class::Class(string name, Traced<Class*> maybeBase,
             NativeConstructor constructor,
             Traced<Layout*> initialLayout) :
  Object(ObjectClass, initialLayout), name_(name)
{
    assert(!Class::ObjectClass || maybeBase);
    assert(!maybeBase || maybeBase->is<Class>());
    assert(initialLayout->subsumes(InitialLayout));
    if (Class::ObjectClass) {
        Root<Value> baseAttr(maybeBase ? maybeBase : None);
        setAttr(BaseAttr, baseAttr);
    }
    if (constructor) {
        constructor_ = constructor;
    } else {
        assert(maybeBase);
        constructor_ = maybeBase->nativeConstructor();
    }
    assert(constructor_);
}

Object* Class::base()
{
    Root<Value> value(getAttr(BaseAttr));
    return value.asObject();
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

bool Class::isDerivedFrom(Class* cls) const
{
    AutoAssertNoGC nogc;
    const Object* obj = this;
    while (obj != cls) {
        obj = obj->getOwnAttr("__base__").toObject();
        if (obj == None)
            return false;
    }
    return true;
}

static bool object_str(TracedVector<Value> args, Root<Value>& resultOut)
{
    Object* obj = args[0].toObject();
    ostringstream s;
    s << "<" << obj->type()->name();
    s << " at 0x" << hex << reinterpret_cast<uintptr_t>(obj) << ">";
    resultOut = String::get(s.str());
    return true;
}

static bool object_dump(TracedVector<Value> args, Root<Value>& resultOut)
{
    Object* obj = args[0].toObject();
    obj->print(cout);
    resultOut = None;
    return true;
}

void initObject()
{
    Object::InitialLayout.init(gc.create<Layout>(Layout::None, ClassAttr));
    Class::InitialLayout.init(
        gc.create<Layout>(Object::InitialLayout, BaseAttr));

    assert(Object::InitialLayout->lookupName(ClassAttr) == Object::ClassSlot);

    Root<Class*> objectBase(nullptr);
    Object::ObjectClass.init(gc.create<Class>("Object", objectBase,
                                              Object::createInstance));
    NoneObject::ObjectClass.init(gc.create<Class>("None"));
    Class::ObjectClass.init(gc.create<Class>("Class"));

    None.init(gc.create<NoneObject>());

    Class::ObjectClass->init(Object::ObjectClass);
    Object::ObjectClass->init(None);
    NoneObject::ObjectClass->init(Class::ObjectClass);

    // Create classes for callables here as this is necessary before we can add
    // methods to objects.
    Native::ObjectClass.init(gc.create<Class>("Native"));
    Function::ObjectClass.init(gc.create<Class>("Function"));

    initNativeMethod(Object::ObjectClass, "__str__", object_str, 1);
    initNativeMethod(Object::ObjectClass, "__dump__", object_dump, 1);
}

#ifdef BUILD_TESTS

#include "test.h"
#include "numeric.h"

testcase(object)
{
    Root<Object*> o(Object::create());

    Root<Value> v;
    testFalse(o->hasAttr("foo"));
    testFalse(o->maybeGetAttr("foo", v));

    Root<Value> value(Integer::get(1));
    o->setAttr("foo", value);
    testTrue(o->hasAttr("foo"));
    testTrue(o->maybeGetAttr("foo", v));
    Root<Object*> obj(v.toObject());
    testEqual(v.toObject()->as<Integer>()->value(), 1);

    value = Integer::get(2);
    o->setAttr("bar", value);
    value = Integer::get(3);
    o->setAttr("baz", value);
    testTrue(o->hasAttr("foo"));
    testTrue(o->hasAttr("bar"));
    testTrue(o->hasAttr("baz"));

    testFalse(o->maybeDelOwnAttr("missing"));

    testTrue(o->maybeDelOwnAttr("bar"));
    testTrue(o->hasAttr("foo"));
    testFalse(o->hasAttr("bar"));
    testTrue(o->hasAttr("baz"));
    testEqual(o->getAttr("foo").toObject()->as<Integer>()->value(), 1);
    testEqual(o->getAttr("baz").toObject()->as<Integer>()->value(), 3);

    testTrue(o->maybeDelOwnAttr("baz"));
    testTrue(o->hasAttr("foo"));
    testFalse(o->hasAttr("baz"));
    testEqual(o->getAttr("foo").toObject()->as<Integer>()->value(), 1);

    testTrue(o->maybeDelOwnAttr("foo"));
    testFalse(o->hasAttr("foo"));

    testFalse(o->maybeDelOwnAttr("foo"));

    testFalse(None->isTrue());
    testFalse(Integer::get(0).isTrue());
    testTrue(Integer::get(1).isTrue());
}

#endif
