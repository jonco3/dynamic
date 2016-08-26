#include "set.h"

#include "callable.h"
#include "exception.h"
#include "interp.h"
#include "list.h"
#include "numeric.h"
#include "repr.h"

#include <stdexcept>

GlobalRoot<Class*> Set::ObjectClass;

template <typename T>
static bool set_len(NativeArgs args, MutableTraced<Value> resultOut)
{
    Stack<T*> set(args[0].as<T>());;
    resultOut = Integer::get(set->len());
    return true;
}

template <typename T>
static bool set_contains(NativeArgs args, MutableTraced<Value> resultOut)
{
    Stack<T*> set(args[0].as<T>());;
    resultOut = Boolean::get(set->contains(args[1]));
    return true;
}

template <typename T>
static bool set_add(NativeArgs args, MutableTraced<Value> resultOut)
{
    Stack<T*> set(args[0].as<T>());;
    set->add(args[1]);
    return true;
}

template <typename T>
static bool set_keys(NativeArgs args, MutableTraced<Value> resultOut)
{
    Stack<T*> set(args[0].as<T>());;
    resultOut = set->keys();
    return true;
}

template <typename T>
static void SetInit(const char* name)
{
    T::ObjectClass.init(Class::createNative<T>(name));
    initNativeMethod(T::ObjectClass, "__len__", set_len<T>, 1);
    initNativeMethod(T::ObjectClass, "__contains__", set_contains<T>, 2);
    initNativeMethod(T::ObjectClass, "add", set_add<T>, 2);
    initNativeMethod(T::ObjectClass, "keys", set_keys<T>, 1);
    // __eq__ and __ne__ are supplied by internals/internal.py
}

void Set::init()
{
    SetInit<Set>("set");
}

Set::Set()
  : Object(ObjectClass)
{}

Set::Set(const TracedVector<Value>& values)
  : Object(ObjectClass), elements_(values.size() / 2)
{
    for (unsigned i = 0; i < values.size(); ++i)
        elements_.insert(values[i]);
}

Set::Set(Traced<Class*> cls)
  : Object(cls)
{
    assert(cls->isDerivedFrom(ObjectClass));
}

void Set::traceChildren(Tracer& t)
{
    Object::traceChildren(t);
    for (auto i = elements_.begin(); i != elements_.end(); ++i) {
        Value key = *i;
#ifdef DEBUG
        Value prior = key;
#endif
        gc.traceUnbarriered(t, &key);
        assert(key == prior);
    }
}

void Set::print(ostream& s) const
{
    s << "{";
    for (auto i = elements_.begin(); i != elements_.end(); ++i) {
        if (i != elements_.begin())
            s << ", ";
        s << *i;
    }
    s << "}";
}

void Set::add(Traced<Value> element)
{
    elements_.insert(element);
}

void Set::clear()
{
    elements_.clear();
}

Value Set::keys() const
{
    // todo: should be some kind of iterator?
    Stack<Tuple*> keys(Tuple::getUninitialised(elements_.size()));
    size_t index = 0;
    for (const auto& i : elements_)
        keys->initElement(index++, i);
    return Value(keys);
}

