#include "dict.h"

#include "callable.h"
#include "exception.h"
#include "interp.h"
#include "list.h"
#include "numeric.h"
#include "repr.h"

#include <stdexcept>

GlobalRoot<Class*> Dict::ObjectClass;
GlobalRoot<Class*> DictView::ObjectClass;

template <typename T>
static bool dict_len(NativeArgs args, MutableTraced<Value> resultOut)
{
    Stack<T*> dict(args[0].as<T>());;
    resultOut = Integer::get(dict->len());
    return true;
}

template <typename T>
static bool dict_contains(NativeArgs args,
                          MutableTraced<Value> resultOut)
{
    Stack<T*> dict(args[0].as<T>());;
    resultOut = Boolean::get(dict->contains(args[1]));
    return true;
}

template <typename T>
static bool dict_getitem(NativeArgs args,
                         MutableTraced<Value> resultOut)
{
    Stack<T*> dict(args[0].as<T>());;

    if (!dict->getitem(args[1], resultOut)) {
        resultOut = gc.create<KeyError>(repr(args[1]));
        return false;
    }

    return true;
}

template <typename T>
static bool dict_setitem(NativeArgs args, MutableTraced<Value> resultOut)
{
    Stack<T*> dict(args[0].as<T>());;
    dict->setitem(args[1], args[2]);
    resultOut = args[2];
    return true;
}

template <typename T>
static bool dict_delitem(NativeArgs args, MutableTraced<Value> resultOut)
{
    Stack<T*> dict(args[0].as<T>());;
    return dict->delitem(args[1], resultOut);
}

template <typename T>
static bool dict_keys(NativeArgs args, MutableTraced<Value> resultOut)
{
    Stack<T*> dict(args[0].as<T>());;
    resultOut = dict->keys();
    return true;
}

template <typename T>
static bool dict_values(NativeArgs args, MutableTraced<Value> resultOut)
{
    Stack<T*> dict(args[0].as<T>());;
    resultOut = dict->values();
    return true;
}

template <typename T>
static void DictInit(const char* name)
{
    T::ObjectClass.init(Class::createNative<T>(name));
    initNativeMethod(T::ObjectClass, "__len__", dict_len<T>, 1);
    initNativeMethod(T::ObjectClass, "__contains__", dict_contains<T>, 2);
    initNativeMethod(T::ObjectClass, "__getitem__", dict_getitem<T>, 2);
    initNativeMethod(T::ObjectClass, "__setitem__", dict_setitem<T>, 3);
    initNativeMethod(T::ObjectClass, "__delitem__", dict_delitem<T>, 2);
    initNativeMethod(T::ObjectClass, "keys", dict_keys<T>, 1);
    initNativeMethod(T::ObjectClass, "values", dict_values<T>, 1);
    // __eq__ and __ne__ are supplied by internals/internal.py
}

void Dict::init()
{
    DictInit<Dict>("dict");
}

Dict::Dict()
  : Object(ObjectClass)
{}

Dict::Dict(const TracedVector<Value>& values)
  : Object(ObjectClass), entries_(values.size() / 2)
{
    unsigned entryCount = values.size() / 2;
    for (unsigned i = 0; i < entryCount; ++i)
        entries_[values[i * 2]] = values[i * 2 + 1];
}

Dict::Dict(Traced<Class*> cls)
  : Object(cls)
{
    assert(cls->isDerivedFrom(ObjectClass));
}

void Dict::traceChildren(Tracer& t)
{
    Object::traceChildren(t);
    for (auto i = entries_.begin(); i != entries_.end(); ++i) {
        Value key = i->first;
#ifdef DEBUG
        Value prior = key;
#endif
        gc.traceUnbarriered(t, &key);
        assert(key == prior);
        gc.trace(t, &i->second);
    }
}

void Dict::print(ostream& s) const
{
    s << "{";
    for (auto i = entries_.begin(); i != entries_.end(); ++i) {
        if (i != entries_.begin())
            s << ", ";
        s << i->first << ": " << i->second;
    }
    s << "}";
}

bool Dict::getitem(Traced<Value> key, MutableTraced<Value> resultOut) const
{
    auto i = entries_.find(key);
    if (i == entries_.end())
        return false;

    resultOut = i->second;
    return true;
}

void Dict::setitem(Traced<Value> key, Traced<Value> value)
{
    entries_[key] = value;
}

bool Dict::delitem(Traced<Value> key, MutableTraced<Value> resultOut)
{
    auto i = entries_.find(key);
    if (i == entries_.end()) {
        resultOut = gc.create<KeyError>(repr(key));
        return false;
    }

    entries_.erase(i);
    resultOut = None;
    return true;
}

void Dict::clear()
{
    entries_.clear();
}

Value Dict::keys() const
{
    // todo: should be some kind of iterator?
    Stack<Tuple*> keys(Tuple::get(entries_.size()));
    size_t index = 0;
    for (const auto& i : entries_)
        keys->initElement(index++, i.first);
    return Value(keys);
}

Value Dict::values() const
{
    // todo: should be some kind of iterator?
    Stack<Tuple*> values(Tuple::get(entries_.size()));
    size_t index = 0;
    for (const auto& i : entries_)
        values->initElement(index++, i.second);
    return Value(values);
}

inline Name internStringValue(Traced<Value> v)
{
    return internString(v.as<String>()->value());
}

size_t Dict::ValueHash::operator()(Value vArg) const
{
    Stack<Value> value(vArg);
    Stack<Value> hashFunc;
    Stack<Value> result;
    if (!value.maybeGetAttr(Names::__hash__, hashFunc)) {
        result = gc.create<TypeError>("Object has no __hash__ method");
        throw PythonException(result);
    }

    interp->pushStack(value);
    if (!interp->call(hashFunc, 1, result))
        throw PythonException(result);

    if (result.isInt32())
        return result.asInt32();

    if (!result.is<Integer>()) {
        result = gc.create<TypeError>("__hash__ method should return an int");
        throw PythonException(result);
    }

    return result.as<Integer>()->value().get_ui(); // todo: truncates result
}

bool Dict::ValuesEqual::operator()(Value a, Value b) const
{
    Stack<Value> result;
    Stack<Value> eqFunc;
    if (!a.maybeGetAttr(Names::__eq__, eqFunc)) {
        result = gc.create<TypeError>("Object has no __eq__ method");
        throw PythonException(result);
    }

    interp->pushStack(a);
    interp->pushStack(b);
    if (!interp->call(eqFunc, 2, result))
        throw PythonException(result);

    Stack<Object*> obj(result.toObject());
    if (!obj->is<Boolean>()) {
        result = gc.create<TypeError>("__eq__ method should return a bool");
        throw PythonException(result);
    }

    return obj->as<Boolean>()->boolValue();
}

void DictView::init()
{
    DictInit<DictView>("dictview"); // We may want to lie about class name.
}

DictView::DictView(Traced<Object*> obj)
  : Object(ObjectClass), object_(obj), layout_(obj->layout())
{}

void DictView::traceChildren(Tracer& t)
{
    Object::traceChildren(t);
    gc.trace(t, &object_);
    gc.trace(t, &layout_);
}

void DictView::print(ostream& s) const
{
    s << *object_;
}

void DictView::checkMap() const
{
    if (object_->layout() != layout_)
        slots_.clear();
}

size_t DictView::len() const
{
    return object_->layout()->slotCount();
}

int DictView::findSlot(Traced<Value> key) const
{
    if (!key.is<String>())
        return BadName;

    // todo: check valid name?

    // todo: GC hazard if we move objects
    Name name = internStringValue(key);
    return findSlot(name);
}

int DictView::findSlot(Name name) const
{
    checkMap();
    auto i = slots_.find(name);
    if (i != slots_.end())
        return i->second;

    int slot = object_->findOwnAttr(name);
    slots_[name] = slot;
    return slot;
}

bool DictView::contains(Traced<Value> key) const
{
    return findSlot(key) != Layout::NotFound;
}

bool DictView::getitem(Traced<Value> key, MutableTraced<Value> resultOut) const
{
    int slot = findSlot(key);
    if (slot < 0)
        return false;

    resultOut = object_->getSlot(slot);
    return true;
}

void DictView::setitem(Traced<Value> key, Traced<Value> value)
{
    if (!key.is<String>())
        return;

    // todo: GC hazard if we move objects
    Name name = internStringValue(key);

    int slot = findSlot(name);
    if (slot == Layout::NotFound) {
        object_->setAttr(name, value);
        return;
    }

    object_->setSlot(slot, value);
}

bool DictView::delitem(Traced<Value> key, MutableTraced<Value> resultOut)
{
    if (!key.is<String>())
        return false;

    // todo: check valid name?

    // todo: GC hazard if we move objects
    Name name = internStringValue(key);
    return object_->maybeDelOwnAttr(name);
}

Value DictView::keys() const
{
    // todo: should be some kind of iterator?
    Stack<Tuple*> keys(Tuple::get(len()));
    Stack<Layout*> layout(object_->layout());
    size_t index = 0;
    while (layout != Layout::Empty) {
        keys->initElement(index++, layout->name());
        layout = layout->parent();
    }
    return Value(keys);
}

Value DictView::values() const
{
    // todo: should be some kind of iterator?
    Stack<Tuple*> values(Tuple::get(len()));
    for (size_t index = 0; index < len(); index++)
        values->initElement(index, object_->getSlot(index));
    return Value(values);
}
