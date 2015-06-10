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
    class_ = cls;
    if (layout_ == Layout::Empty)
        return;

    slots_.resize(layout_->slotCount());
    for (unsigned i = 0; i < layout_->slotCount(); ++i)
        slots_[i] = UninitializedSlot;
}

void Object::extend(Traced<Layout*> layout)
{
    assert(layout);
#ifdef DEBUG
    Root<Layout*> current(layout_);
    assert(layout->subsumes(current));
#endif

    unsigned count = 0;
    for (Layout* l = layout; l != layout_; l = l->parent()) {
        assert(layout);
        ++count;
    }
    unsigned initialSize = slots_.size();
    slots_.resize(initialSize + count);
    for (unsigned i = initialSize; i < layout->slotCount(); ++i)
        slots_[i] = UninitializedSlot;
    layout_ = layout;
}

bool Object::getSlot(Name name, int slot, Root<Value>& valueOut) const
{
    assert(slot >= 0 && static_cast<size_t>(slot) < slots_.size());
    if (slots_[slot] == Value(UninitializedSlot))
        return false;
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

int Object::findOwnAttr(Name name) const
{
    return layout_->lookupName(name);
}

bool Object::hasOwnAttr(Name name) const
{
    return findOwnAttr(name) != Layout::NotFound;
}

bool Object::maybeGetOwnAttr(Name name, Root<Value>& valueOut) const
{
    int slot = layout_->lookupName(name);
    if (slot == Layout::NotFound)
        return false;
    return getSlot(name, slot, valueOut);
}

bool Object::hasAttr(Name name) const
{
    AutoAssertNoGC nogc;

    if (hasOwnAttr(name))
        return true;

    if (!isInstanceOf(Class::ObjectClass))
        return type()->hasAttr(name);

    Root<Value> base;
    if (!maybeGetOwnAttr(BaseAttr, base) || base.isNone())
        return false;
    assert(base.isInstanceOf(Class::ObjectClass));
    return base.toObject()->hasAttr(name);
}

bool Object::maybeGetAttr(Name name, Root<Value>& valueOut) const
{
    AutoAssertNoGC nogc;

    if (maybeGetOwnAttr(name, valueOut))
        return true;

    if (!isInstanceOf(Class::ObjectClass))
        return type()->maybeGetAttr(name, valueOut);

    Root<Value> base;
    if (!maybeGetOwnAttr(BaseAttr, base) || base.isNone())
        return false;
    assert(base.isInstanceOf(Class::ObjectClass));
    return base.toObject()->maybeGetAttr(name, valueOut);
}

Value Object::getAttr(Name name) const
{
    Root<Value> value;
    bool ok = maybeGetAttr(name, value);
    assert(ok);
    (void)ok;
    return value;
}

void Object::initAttr(Name name, Traced<Value> value)
{
    assert(!hasOwnAttr(name));
    setAttr(name, value);
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
        assert(!hasOwnAttr(name));
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
    assert(!hasOwnAttr(name));
    return true;
}

void Object::print(ostream& s) const
{
    s << "<" << type()->name();
    s << " object at 0x" << hex << reinterpret_cast<uintptr_t>(this) << ">";
}

void Object::dump(ostream& s) const
{
    s << type()->name();
    s << " object at 0x" << hex << reinterpret_cast<uintptr_t>(this);
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

bool Object::isInstanceOf(Class* cls) const
{
    AutoAssertNoGC nogc;
    return type()->isDerivedFrom(cls);
}

void Object::traceChildren(Tracer& t)
{
    gc.trace(t, &class_);
    gc.trace(t, &layout_);
    for (auto i = slots_.begin(); i != slots_.end(); ++i)
        gc.trace(t, &*i);
}

/* static */ Class* Class::createNative(string name,
                                        NativeFunc newFunc, unsigned maxArgs,
                                        Traced<Class*> base)
{
    Root<Class*> cls(gc.create<Class>(name, base));
    initNativeMethod(cls, "__new__", newFunc, 1, maxArgs);
    return cls;
}


Class::Class(string name, Traced<Class*> base, Traced<Layout*> initialLayout) :
  Object(ObjectClass, initialLayout), name_(name)
{
    // base is null for Object when we are initializing.
    assert(!Class::ObjectClass || base);
    assert(initialLayout->subsumes(InitialLayout));
    if (Class::ObjectClass)
        setAttr(BaseAttr, base);
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
        obj = obj->getAttr("__base__").toObject();
        if (obj == None)
            return false;
    }
    return true;
}

void initNativeMethod(Traced<Object*> cls, Name name, NativeFunc func,
                      unsigned minArgs, unsigned maxArgs)
{
    Root<Value> value(None);
    if (func)
        value = gc.create<Native>(name, func, minArgs, maxArgs);
    cls->initAttr(name, value);
}

bool object_new(TracedVector<Value> args, Root<Value>& resultOut)
{
    // Special case because we can't call Class::createNative in initialization.
    if (!checkInstanceOf(args[0], Class::ObjectClass, resultOut))
        return false;
    Root<Class*> cls(args[0].asObject()->as<Class>());
    resultOut = gc.create<Object>(cls);
    return true;
}

static bool object_repr(TracedVector<Value> args, Root<Value>& resultOut)
{
    Object* obj = args[0].toObject();
    ostringstream s;
    obj->print(s);
    resultOut = String::get(s.str());
    return true;
}

static bool object_dump(TracedVector<Value> args, Root<Value>& resultOut)
{
    Object* obj = args[0].toObject();
    obj->dump(cout);
    resultOut = None;
    return true;
}

static bool object_eq(TracedVector<Value> args, Root<Value>& resultOut)
{
    resultOut = Boolean::get(args[0].toObject() == args[1].toObject());
    return true;
}

static bool object_ne(TracedVector<Value> args, Root<Value>& resultOut)
{
    resultOut = Boolean::get(args[0].toObject() != args[1].toObject());
    return true;
}

static bool object_hash(TracedVector<Value> args, Root<Value>& resultOut)
{
    resultOut = Integer::get((int32_t)(uintptr_t)args[0].toObject());
    return true;
}

void initObject()
{
    Object::InitialLayout.init(Layout::Empty);
    Class::InitialLayout.init(Object::InitialLayout->addName(BaseAttr));

    Root<Class*> objectBase(nullptr);
    Object::ObjectClass.init(gc.create<Class>("object", objectBase));
    NoneObject::ObjectClass.init(gc.create<Class>("None"));
    Class::ObjectClass.init(gc.create<Class>("class"));

    None.init(gc.create<NoneObject>());

    Class::ObjectClass->init(Object::ObjectClass);
    Object::ObjectClass->init(None);
    NoneObject::ObjectClass->init(Object::ObjectClass);

    // Create classes for callables here as this is necessary before we can add
    // methods to objects.
    Native::ObjectClass.init(gc.create<Class>("Native"));
    Function::ObjectClass.init(gc.create<Class>("Function"));

    initNativeMethod(Object::ObjectClass, "__new__", object_new, 1, -1);
    initNativeMethod(Object::ObjectClass, "__repr__", object_repr, 1);
    initNativeMethod(Object::ObjectClass, "__dump__", object_dump, 1);
    initNativeMethod(Object::ObjectClass, "__eq__", object_eq, 2);
    initNativeMethod(Object::ObjectClass, "__ne__", object_ne, 2);
    initNativeMethod(Object::ObjectClass, "__hash__", object_hash, 1);
    initNativeMethod(NoneObject::ObjectClass, "__new__", nullptr, 1, -1);
    initNativeMethod(Class::ObjectClass, "__new__", nullptr, 1, -1);
}
