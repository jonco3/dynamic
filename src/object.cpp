#include "object.h"

#include "bool.h"
#include "callable.h"
#include "exception.h"
#include "list.h"
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
    Stack<Layout*> current(layout_);
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

bool Object::getSlot(Name name, int slot, MutableTraced<Value> valueOut) const
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

bool Object::maybeGetOwnAttr(Name name, MutableTraced<Value> valueOut) const
{
    int slot = layout_->lookupName(name);
    if (slot == Layout::NotFound)
        return false;
    return getSlot(name, slot, valueOut);
}

bool Object::hasAttr(Name name) const
{
    AutoAssertNoGC nogc;

    const Object* obj = this;

    for (;;) {
        if (obj->hasOwnAttr(name))
            return true;

        if (!obj->is<Class>()) {
            obj = obj->type();
        } else {
            obj = obj->as<Class>()->base();
            if (obj == None)
                return false;
            assert(obj->is<Class>());
        }
    }
}

bool Object::maybeGetAttr(Name name, MutableTraced<Value> valueOut) const
{
    AutoAssertNoGC nogc;

    const Object* obj = this;

    for (;;) {
        if (obj->maybeGetOwnAttr(name, valueOut))
            return true;

        if (!obj->is<Class>()) {
            obj = obj->type();
        } else {
            obj = obj->as<Class>()->base();
            if (obj == None)
                return false;
            assert(obj->is<Class>());
        }
    }
}

Value Object::getAttr(Name name) const
{
    Stack<Value> value;
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
        !(is<Integer>() && as<Integer>()->value() == 0);
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
    Stack<Class*> cls(gc.create<Class>(name, base));
    initNativeMethod(cls, "__new__", newFunc, 1, maxArgs);
    return cls;
}


Class::Class(string name, Traced<Class*> base, Traced<Layout*> initialLayout) :
  Object(ObjectClass, initialLayout), name_(name), base_(base)
{
    // base is null for Object when we are initializing.
    assert(!Class::ObjectClass || base);
    assert(initialLayout->subsumes(InitialLayout));
}

void Class::init(Traced<Object*> base)
{
    Object::init(ObjectClass);
    base_ = base;
}

void Class::traceChildren(Tracer& t)
{
    Object::traceChildren(t);
    gc.trace(t, &base_);
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
        obj = obj->as<Class>()->base();
        if (obj == None)
            return false;
    }
    return true;
}

void initNativeMethod(Traced<Object*> cls, const string& name,
                      NativeFunc func,
                      unsigned minArgs, unsigned maxArgs)
{
    Stack<Value> value(None);
    if (func)
        value = gc.create<Native>(name, func, minArgs, maxArgs);
    cls->initAttr(name, value);
}

bool object_new(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    // Special case because we can't call Class::createNative in initialization.
    if (!checkInstanceOf(args[0], Class::ObjectClass, resultOut))
        return false;
    Stack<Class*> cls(args[0].asObject()->as<Class>());
    resultOut = gc.create<Object>(cls);
    return true;
}

static bool object_repr(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    Object* obj = args[0].toObject();
    ostringstream s;
    obj->print(s);
    resultOut = String::get(s.str());
    return true;
}

static bool object_dump(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    Object* obj = args[0].toObject();
    obj->dump(cout);
    resultOut = None;
    return true;
}

static bool object_eq(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    resultOut = Boolean::get(args[0].toObject() == args[1].toObject());
    return true;
}

static bool object_ne(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    resultOut = Boolean::get(args[0].toObject() != args[1].toObject());
    return true;
}

static bool object_hash(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    resultOut = Integer::get((int32_t)(uintptr_t)args[0].toObject());
    return true;
}

void initObject()
{

    Object::InitialLayout.init(Layout::Empty);
    Class::InitialLayout.init(Object::InitialLayout);

    Stack<Class*> objectBase(nullptr);
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
    Function::ObjectClass.init(gc.create<Class>("function"));

    initNativeMethod(Object::ObjectClass, "__new__", object_new, 1, -1);
    initNativeMethod(Object::ObjectClass, "__repr__", object_repr, 1);
    initNativeMethod(Object::ObjectClass, "__dump__", object_dump, 1);
    initNativeMethod(Object::ObjectClass, "__eq__", object_eq, 2);
    initNativeMethod(Object::ObjectClass, "__ne__", object_ne, 2);
    initNativeMethod(Object::ObjectClass, "__hash__", object_hash, 1);
    initNativeMethod(NoneObject::ObjectClass, "__new__", nullptr, 1, -1);
    initNativeMethod(Class::ObjectClass, "__new__", nullptr, 1, -1);
}

/*
 * Attribute lookup
 *
 * From: http://www.cafepy.com/article/python_attributes_and_methods/python_attributes_and_methods.html
 *
 * When retrieving an attribute from an object (print objectname.attrname)
 * Python follows these steps:
 *
 *     If attrname is a special (i.e. Python-provided) attribute for objectname,
 *     return it.
 *
 *     Check objectname.__class__.__dict__ for attrname. If it exists and is a
 *     data-descriptor, return the descriptor result. Search all bases of
 *     objectname.__class__ for the same case.
 *
 *     Check objectname.__dict__ for attrname, and return if found. If
 *     objectname is a class, search its bases too. If it is a class and a
 *     descriptor exists in it or its bases, return the descriptor result.
 *
 *     Check objectname.__class__.__dict__ for attrname. If it exists and is a
 *     non-data descriptor, return the descriptor result. If it exists, and is
 *     not a descriptor, just return it. If it exists and is a data descriptor,
 *     we shouldn't be here because we would have returned at point 2. Search
 *     all bases of objectname.__class__ for same case.
 *
 *     Raise AttributeError
 */

static Name foo;

static bool isNonDataDescriptor(Traced<Value> value)
{
    if (!value.isObject())
        return false;

    // Don't check for descriptors here as that could go recursive.
    Stack<Object*> obj(value.asObject());
    return obj->hasAttr(Name::__get__);
}

static bool isDataDescriptor(Traced<Value> value)
{
    return isNonDataDescriptor(value) &&
        value.asObject()->hasAttr(Name::__set__);
}

enum class FindResult
{
    NotFound,
    FoundValue,
    FoundDescriptor
};

static FindResult findAttrForGet(Traced<Value> value, Name name,
                                 MutableTraced<Value> resultOut)
{
    Stack<Class*> cls(value.type());
    Stack<Value> classAttr;
    bool foundOnClass = cls->maybeGetAttr(name, classAttr);

    if (foundOnClass && isDataDescriptor(classAttr)) {
        resultOut = classAttr;
        return FindResult::FoundDescriptor;
    }

    Stack<Object*> obj(value.maybeObject());
    if (obj) {
        if (obj->is<Class>()) {
            if (obj->maybeGetAttr(name, resultOut)) {
                return isNonDataDescriptor(resultOut) ?
                    FindResult::FoundDescriptor : FindResult::FoundValue;
            }
        } else {
            if (obj->maybeGetOwnAttr(name, resultOut)) {
                return FindResult::FoundValue;
            }
        }
    }

    if (foundOnClass) {
        resultOut = classAttr;
        return isNonDataDescriptor(resultOut) ?
            FindResult::FoundDescriptor : FindResult::FoundValue;
    }

    return FindResult::NotFound;
}

static bool raiseAttrError(Traced<Value> value, Name name,
                           MutableTraced<Value> resultOut)
{
    Stack<Class*> cls(value.type());
    string message =
        "'" + cls->name() + "' object has no attribute '" + name + "'";
    resultOut = gc.create<AttributeError>(message);
    return false;
}

static bool getSpecialAttr(Traced<Value> value, Name name,
                           MutableTraced<Value> resultOut)
{
    if (name == Name::__class__) {
        resultOut = value.type();
        return true;
    } else if (value.is<Class>()) {
        Stack<Object*> obj(value.asObject());
        if (name == Name::__name__) {
            resultOut = gc.create<String>(obj->as<Class>()->name());
            return true;
        } else if (name == Name::__bases__) {
            RootArray<Value, 1> bases;
            bases[0] = obj->as<Class>()->base();
            resultOut = gc.create<Tuple>(bases);
            return true;
        }
    }

    return false;
}

static bool getAttrOrDescriptor(Traced<Value> value, Name name,
                                MutableTraced<Value> resultOut, bool& isDescriptor)
{
    if (getSpecialAttr(value, name, resultOut))
        return true;

    FindResult r = findAttrForGet(value, name, resultOut);
    if (r == FindResult::NotFound)
        return raiseAttrError(value, name, resultOut);

    isDescriptor = r == FindResult::FoundDescriptor;
    return true;
}

static bool getDescriptorValue(Traced<Value> value, MutableTraced<Value> valueInOut)
{
    Stack<Object*> desc(valueInOut.asObject());
    Stack<Value> func(desc->getAttr(Name::__get__));
    bool isClass = value.is<Class>();
    RootArray<Value, 3> args;
    args[0] = desc;
    args[1] = isClass ? None : value;
    args[2] = isClass ? value.get() : value.type();
    Stack<Value> result;
    bool ok = Interpreter::instance().call(func, args, result);
    valueInOut = result;
    return ok; // todo: drive MutableTraced through interpreter interface
}

bool getAttr(Traced<Value> value, Name name, MutableTraced<Value> resultOut)
{
    bool isDescriptor;
    if (!getAttrOrDescriptor(value, name, resultOut, isDescriptor))
        return false;

    if (isDescriptor && !getDescriptorValue(value, resultOut))
        return false;

    return true;
}

/*
 * Get the value of an attribute that might be a descript.  If it is a
 * descriptor that is also callable, don't call its __get__ method but indicate
 * this by setting isCallableDescriptor.
 */
bool getMethodAttr(Traced<Value> value, Name name, MutableTraced<Value> resultOut,
                   bool& isCallableDescriptor)
{
    bool isDescriptor;
    if (!getAttrOrDescriptor(value, name, resultOut, isDescriptor))
        return false;

    isCallableDescriptor = false;
    if (isDescriptor) {
        if (resultOut.is<Function>() || resultOut.is<Native>()) {
            isCallableDescriptor = true;
        } else {
            if (!getDescriptorValue(value, resultOut))
                return false;
        }
    }

    return true;
}

/*
 *  Now, the steps Python follows when setting a user-defined attribute
 *  (objectname.attrname = something):
 *
 *     Check objectname.__class__.__dict__ for attrname. If it exists and is a
 *     data-descriptor, use the descriptor to set the value. Search all bases of
 *     objectname.__class__ for the same case.
 *
 *     Insert something into objectname.__dict__ for key "attrname".
 */

FindResult findAttrForSetOrDelete(Name name, Traced<Object*> obj,
                                  MutableTraced<Value> resultOut)
{
    // todo: check for specials?

    Stack<Class*> cls(obj->type());

    Stack<Value> classAttr;
    bool foundOnClass = cls->maybeGetAttr(name, classAttr);

    if (foundOnClass && isDataDescriptor(classAttr)) {
        resultOut = classAttr;
        return FindResult::FoundDescriptor;
    }

    return FindResult::NotFound;
}

bool setAttr(Traced<Object*> obj, Name name, Traced<Value> value,
             MutableTraced<Value> resultOut)
{
    Stack<Value> descValue;
    FindResult r = findAttrForSetOrDelete(name, obj, descValue);
    if (r == FindResult::FoundDescriptor) {
        Stack<Object*> desc(descValue.asObject());
        Stack<Value> func(desc->getAttr(Name::__set__));
        RootArray<Value, 3> args;
        args[0] = desc;
        args[1] = obj;
        args[2] = value.get();
        Stack<Value> result;
        bool ok = Interpreter::instance().call(func, args, result);
        resultOut = result;
        return ok; // todo: drive MutableTraced through interpreter interface
    }

    assert(r == FindResult::NotFound);
    obj->setAttr(name, value);
    resultOut = None;
    return true;
}

bool delAttr(Traced<Object*> obj, Name name, MutableTraced<Value> resultOut)
{
    Stack<Value> descValue;
    FindResult r = findAttrForSetOrDelete(name, obj, descValue);
    if (r == FindResult::FoundDescriptor) {
        Stack<Object*> desc(descValue.asObject());
        Stack<Value> func(desc->getAttr(Name::__delete__));
        RootArray<Value, 2> args;
        args[0] = desc;
        args[1] = obj;
        Stack<Value> result;
        bool ok = Interpreter::instance().call(func, args, result);
        resultOut = result;
        return ok; // todo: drive MutableTraced through interpreter interface
    }

    assert(r == FindResult::NotFound);
    if (!obj->maybeDelOwnAttr(name))
        return raiseAttrError(obj, name, resultOut);

    return true;
}

