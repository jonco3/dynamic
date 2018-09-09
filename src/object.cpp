#include "object.h"

#include "callable.h"
#include "exception.h"
#include "interp.h"
#include "list.h"
#include "numeric.h"
#include "singletons.h"

#include "value-inl.h"

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

Object::Object(Traced<Class*> cls, Traced<Layout*> layout)
  : layout_(layout)
{
    init(cls, layout);
}

void Object::init(Traced<Class*> cls, Traced<Layout*> layout)
{
    assert(layout_);
    assert(layout_->subsumes(InitialLayout));
    slots_.assign(layout_->slotCount(), UninitializedSlot.get());
    class_ = cls;
}

void Object::finishInit(Traced<Class*> cls)
{
    assert(cls);
    assert(!class_);
    class_ = cls;
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

bool Object::hasSlot(int slot) const
{
    assert(slot >= 0 && static_cast<size_t>(slot) < slots_.size());
    return slots_[slot] != Value(UninitializedSlot);
}

bool Object::getSlot(Name name, int slot, MutableTraced<Value> valueOut) const
{
    assert(slot >= 0 && static_cast<size_t>(slot) < slots_.size());
    assert(layout_->lookupName(name) == slot);
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

void Object::setSlot(int slot, Value value)
{
    assert(slot >= 0 && static_cast<size_t>(slot) < slots_.size());
    slots_[slot] = value;
}

int Object::findOwnAttr(Name name) const
{
    int slot = layout_->lookupName(name);
    if (slot != Layout::NotFound && slots_[slot] == Value(UninitializedSlot))
        return Layout::NotFound;

    return slot;
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
        }

        assert(obj->is<Class>());
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
        }

        assert(obj->is<Class>());
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
    Layout* layout = layout_;
    while (layout != Layout::Empty) {
        s << *layout->name() << ": ";
        unsigned slot = layout->slotIndex();
        if (slot < slots_.size())
            s << slots_[slot];
        else
            s << "not present";
        layout = layout->parent();
        assert(layout);
        if (layout != Layout::Empty)
            s << ", ";
    }
    s << " } ";
}

/* static */ bool Object::IsTrue(Traced<Object*> obj)
{
    // the following values are interpreted as false: False, None, numeric zero
    // of all types, and empty strings and containers (including strings,
    // tuples, lists, dictionaries, sets and frozensets).
    // https://docs.python.org/2/reference/expressions.html#boolean-operations
    // todo: call __nonzero__ and __empty__
    return
        obj != None &&
        obj != Boolean::False &&
        !(obj->is<Integer>() && obj->as<Integer>()->value() == 0) &&
        !(obj->is<Float>() && obj->as<Float>()->value() == 0);
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
    gc.traceVector(t, &slots_);
}

/* static */ Class* Class::createNative(string name,
                                        NativeFunc newFunc, unsigned maxArgs,
                                        Traced<Class*> base)
{
    Stack<Class*> cls(gc.create<Class>(name, base, InitialLayout, true));
    initNativeMethod(cls, "__new__", newFunc, 1, maxArgs);
    return cls;
}


Class::Class(string name,
             Traced<Class*> base,
             Traced<Layout*> initialLayout,
             bool final)
  : Object(ObjectClass, initialLayout),
    name_(name),
    final_(final)
{
    // base is null for Object when we are initializing.
    assert(!Class::ObjectClass || base);
    assert(initialLayout->subsumes(InitialLayout));
    if (base)
        initBases(base);
}

void Class::finishInitNoBases()
{
    bases_ = Tuple::Empty;
    Object::finishInit(Class::ObjectClass);
}

void Class::finishInit(Traced<Class*> base)
{
    initBases(base);
    Object::finishInit(Class::ObjectClass);
}

void Class::initBases(Traced<Class*> base)
{
    assert(!bases_);
    assert(base);
    RootVector<Value> bases(1);
    bases[0] = base;
    bases_ = Tuple::get(bases);
}

Tuple* Class::mro()
{
    if (mro_)
        return mro_;

    RootVector<Value> chain;
    chain.push_back(this);

    Stack<Object*> obj(base());
    while (obj != None) {
        Stack<Class*> cls(obj->as<Class>());
        chain.push_back(cls.get());
        obj = cls->base();
    }
    mro_ = Tuple::get(chain);
    return mro_;
}

void Class::traceChildren(Tracer& t)
{
    Object::traceChildren(t);
    gc.trace(t, &bases_);
    gc.trace(t, &mro_);
}

void Class::print(ostream& s) const
{
    s << "Class " << name_;
}

Object* Class::base() const
{
    Stack<Tuple*> bases(bases_);
    assert(bases);

    if (bases->len() == 0)
        return None;

    assert(bases->len() == 1);
    return bases->getitem(0).toObject();;
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

void initAttr(Traced<Object*> cls, const string& nameString, Traced<Value> value)
{
    Name name(internString(nameString));
    cls->initAttr(name, value);
}

void initNativeMethod(Traced<Object*> obj, const string& nameString,
                      NativeFunc func, unsigned minArgs, unsigned maxArgs)
{
    Name name(internString(nameString));

    Stack<Value> value(None);
    if (func) {
        Name funcName = name;
        if (obj->is<Class>())
            funcName = internString(obj->as<Class>()->name() + "." + nameString);
        value = gc.create<Native>(funcName, func, minArgs, maxArgs);
    }
    obj->initAttr(name, value);
}

bool object_new(NativeArgs args, MutableTraced<Value> resultOut)
{
    // Special case because we can't call Class::createNative in initialization.
    if (!checkInstanceOf(args[0], Class::ObjectClass, resultOut))
        return false;
    Stack<Class*> cls(args[0].asObject()->as<Class>());
    resultOut = gc.create<Object>(cls);
    return true;
}

static bool object_repr(NativeArgs args, MutableTraced<Value> resultOut)
{
    Object* obj = args[0].toObject();
    ostringstream s;
    obj->print(s);
    resultOut = String::get(s.str());
    return true;
}

static bool object_dump(NativeArgs args, MutableTraced<Value> resultOut)
{
    Object* obj = args[0].toObject();
    obj->dump(cout);
    resultOut = None;
    return true;
}

static bool object_hash(NativeArgs args, MutableTraced<Value> resultOut)
{
    resultOut = Integer::get((int32_t)(uintptr_t)args[0].toObject());
    return true;
}

void initObject()
{
    Object::InitialLayout.init(Layout::Empty);
    Class::InitialLayout.init(Object::InitialLayout);

    Object::ObjectClass.init(
        gc.create<Class>("object", nullptr, Class::InitialLayout, true));
    NoneObject::ObjectClass.init(
        gc.create<Class>("None", nullptr, Class::InitialLayout, true));
    Class::ObjectClass.init(
        gc.create<Class>("class", nullptr, Class::InitialLayout, true));

    None.init(gc.create<NoneObject>());

    // Create classes for callables here as this is necessary before we can add
    // methods to objects.
    Native::ObjectClass.init(gc.create<Class>("Native"));
    Function::ObjectClass.init(gc.create<Class>("function"));

    String::init();

    Tuple::init();

    Class::ObjectClass->finishInit(Object::ObjectClass);
    NoneObject::ObjectClass->finishInit(Object::ObjectClass);
    Object::ObjectClass->finishInitNoBases(); // Depends on Empty.

    initNativeMethod(Object::ObjectClass, "__new__", object_new, 1, -1);
    initNativeMethod(Object::ObjectClass, "__repr__", object_repr, 1);
    initNativeMethod(Object::ObjectClass, "__dump__", object_dump, 1);
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

static bool isNonDataDescriptor(Traced<Value> value)
{
    if (!value.isObject())
        return false;

    // Don't check for descriptors here as that could go recursive.
    Stack<Object*> obj(value.asObject());
    return obj->hasAttr(Names::__get__);
}

static bool isDataDescriptor(Traced<Value> value)
{
    return isNonDataDescriptor(value) &&
        value.asObject()->hasAttr(Names::__set__);
}

static bool findAttrForGet(Traced<Value> value, Name name,
                           MutableTraced<Value> resultOut,
                           bool& isDescriptor)
{
    Stack<Class*> cls(value.type());
    Stack<Value> classAttr;
    bool foundOnClass = cls->maybeGetAttr(name, classAttr);

    if (foundOnClass && isDataDescriptor(classAttr)) {
        resultOut = classAttr;
        isDescriptor = true;
        return true;
    }

    Stack<Object*> obj(value.maybeObject());
    if (obj) {
        if (obj->is<Class>()) {
            if (obj->maybeGetAttr(name, resultOut)) {
                isDescriptor = isNonDataDescriptor(resultOut);
                return true;
            }
        } else {
            if (obj->maybeGetOwnAttr(name, resultOut)) {
                isDescriptor = false;
                return true;
            }
        }
    }

    if (foundOnClass) {
        resultOut = classAttr;
        isDescriptor = isNonDataDescriptor(resultOut);
        return true;
    }

    isDescriptor = false;
    return false;
}

static bool raiseAttrError(Traced<Value> value, Name name,
                           MutableTraced<Value> resultOut)
{
    Stack<Class*> cls(value.type());
    string message =
        "'" + cls->name() + "' object has no attribute '" + name + "'";
    return Raise<AttributeError>(message, resultOut);
}

static bool getSpecialAttr(Traced<Value> value, Name name,
                           MutableTraced<Value> resultOut)
{
    if (name == Names::__class__) {
        resultOut = value.type();
        return true;
    }

    if (value.is<Class>()) {
        Stack<Class*> cls(value.asObject()->as<Class>());
        if (name == Names::__name__) {
            resultOut = gc.create<String>(cls->name());
            return true;
        }
        if (name == Names::__bases__) {
            resultOut = cls->bases();
            return true;
        }
        if (name == Names::__mro__) {
            resultOut = cls->mro();
            return true;
        }
    }

    return false;
}

static bool getAttrOrDescriptor(Traced<Value> value, Name name,
                                MutableTraced<Value> resultOut,
                                bool& isDescriptor)
{
    if (getSpecialAttr(value, name, resultOut)) {
        isDescriptor = false;
        return true;
    }

    if (!findAttrForGet(value, name, resultOut, isDescriptor))
        return false;

    return true;
}

static bool getDescriptorValue(Traced<Value> value, MutableTraced<Value> valueInOut)
{
    Stack<Object*> desc(valueInOut.asObject());
    Stack<Value> func(desc->getAttr(Names::__get__));
    bool isClass = value.is<Class>();
    return interp->call(func,
                        desc,
                        isClass ? None : value,
                        isClass ? value.get() : value.type(),
                        valueInOut);
}

bool getAttr(Traced<Value> value, Name name, MutableTraced<Value> resultOut)
{
    bool isDescriptor;
    if (!getAttrOrDescriptor(value, name, resultOut, isDescriptor))
        return raiseAttrError(value, name, resultOut);

    if (!isDescriptor)
        return true;

    return getDescriptorValue(value, resultOut);
}

/*
 * Get the value of an attribute that might be a descriptor.  If it is a
 * callable descriptor of a type we understand (either a function or a native),
 * don't call its __get__ method but indicate this by setting
 * isCallableDescriptor.
 */
bool getMethodAttr(Traced<Value> value, Name name, StackMethodAttr& resultOut)
{
    bool isDescriptor;
    if (!getAttrOrDescriptor(value, name, resultOut.method, isDescriptor))
        return false;

    if (!isDescriptor) {
        resultOut.isCallable = false;
        return true;
    }

    resultOut.isCallable = resultOut.method.is<Function>() ||
                           resultOut.method.is<Native>();
    if (resultOut.isCallable)
        return true;

    return getDescriptorValue(value, resultOut.method);
}

/*
 * Get a special method.  For some reason python looks at the class but not at
 * the object for these.
 */
bool getSpecialMethodAttr(Traced<Value> value, Name name,
                          StackMethodAttr& resultOut)
{
    assert(isSpecialName(name));
    Stack<Value> type(value.type());
    return getMethodAttr(type, name, resultOut);
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

bool findDescriptorForSetOrDelete(Name name, Traced<Object*> obj,
                                  MutableTraced<Value> resultOut)
{
    // todo: check for specials?

    Stack<Class*> cls(obj->type());

    Stack<Value> classAttr;
    bool foundOnClass = cls->maybeGetAttr(name, classAttr);

    if (!foundOnClass || !isDataDescriptor(classAttr))
        return false;

    resultOut = classAttr;
    return true;
}

bool setAttr(Traced<Object*> obj, Name name, Traced<Value> value,
             MutableTraced<Value> resultOut)
{
    Stack<Value> descValue;
    if (!findDescriptorForSetOrDelete(name, obj, descValue)) {
        obj->setAttr(name, value);
        resultOut = None;
        return true;
    }

    Stack<Object*> desc(descValue.asObject());
    Stack<Value> func(desc->getAttr(Names::__set__));
    return interp->call(func, desc, obj, value, resultOut);
}

bool delAttr(Traced<Object*> obj, Name name, MutableTraced<Value> resultOut)
{
    Stack<Value> descValue;
    if (!findDescriptorForSetOrDelete(name, obj, descValue)) {
        if (!obj->maybeDelOwnAttr(name))
            return raiseAttrError(obj, name, resultOut);

        return true;
    }

    Stack<Object*> desc(descValue.asObject());
    Stack<Value> func(desc->getAttr(Names::__delete__));
    return interp->call(func, desc, obj, resultOut);
}
