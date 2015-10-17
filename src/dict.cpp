#include "dict.h"

#include "callable.h"
#include "exception.h"
#include "interp.h"
#include "list.h"
#include "numeric.h"
#include "repr.h"

#include <stdexcept>

GlobalRoot<Class*> Dict::ObjectClass;

static bool dict_len(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    Dict* dict = args[0].as<Dict>();
    return dict->len(resultOut);
}

static bool dict_contains(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    Dict* dict = args[0].as<Dict>();
    return dict->contains(args[1], resultOut);
}

static bool dict_getitem(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    Dict* dict = args[0].as<Dict>();
    return dict->getitem(args[1], resultOut);
}

static bool dict_setitem(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    Dict* dict = args[0].as<Dict>();
    return dict->setitem(args[1], args[2], resultOut);
}

static bool dict_delitem(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    Dict* dict = args[0].as<Dict>();
    return dict->delitem(args[1], resultOut);
}

static bool dict_keys(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    Dict* dict = args[0].as<Dict>();
    return dict->keys(resultOut);
}

static bool dict_values(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    Dict* dict = args[0].as<Dict>();
    return dict->values(resultOut);
}

void Dict::init()
{
    ObjectClass.init(Class::createNative<Dict>("dict"));
    initNativeMethod(ObjectClass, "__len__", dict_len, 1);
    initNativeMethod(ObjectClass, "__contains__", dict_contains, 2);
    initNativeMethod(ObjectClass, "__getitem__", dict_getitem, 2);
    initNativeMethod(ObjectClass, "__setitem__", dict_setitem, 3);
    initNativeMethod(ObjectClass, "__delitem__", dict_delitem, 2);
    initNativeMethod(ObjectClass, "keys", dict_keys, 1);
    initNativeMethod(ObjectClass, "values", dict_values, 1);
    // __eq__ and __ne__ are supplied by lib/internal.py
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

bool Dict::len(MutableTraced<Value> resultOut)
{
    resultOut = Integer::get(entries_.size());
    return true;
}

bool Dict::contains(Traced<Value> key, MutableTraced<Value> resultOut)
{
    bool found = entries_.find(key) != entries_.end();
    resultOut = Boolean::get(found);
    return true;
}

bool Dict::getitem(Traced<Value> key, MutableTraced<Value> resultOut)
{
    auto i = entries_.find(key);
    if (i == entries_.end()) {
        resultOut = gc.create<KeyError>(repr(key));
        return false;
    }

    resultOut = i->second;
    return true;
}

bool Dict::setitem(Traced<Value> key, Traced<Value> value, MutableTraced<Value> resultOut)
{
    entries_[key] = value;
    resultOut = value;
    return true;
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

bool Dict::keys(MutableTraced<Value> resultOut)
{
    // todo: should be some kind of iterator?
    Stack<Tuple*> keys(Tuple::createUninitialised(entries_.size()));
    size_t index = 0;
    for (const auto& i : entries_)
        keys->initElement(index++, i.first);
    resultOut = Value(keys);
    return true;
}

bool Dict::values(MutableTraced<Value> resultOut)
{
    // todo: should be some kind of iterator?
    Stack<Tuple*> values(Tuple::createUninitialised(entries_.size()));
    size_t index = 0;
    for (const auto& i : entries_)
        values->initElement(index++, i.second);
    resultOut = Value(values);
    return true;
}

// Wrap a python error in a C++ exception to propagate it out of STL machinery.
struct PythonException : public runtime_error
{
    PythonException(Value result)
      : runtime_error("python exception"), result_(result)
    {}

    Value result() const { return result_; }

  private:
    Value result_;
    AutoAssertNoGC nogc_;
};

size_t Dict::ValueHash::operator()(Value v) const
{
    Stack<Value> result;
    Stack<Value> hashFunc;
    if (!v.maybeGetAttr(Name::__hash__, hashFunc)) {
        result = gc.create<TypeError>("Object has no __hash__ method");
        throw PythonException(result);
    }

    interp->pushStack(v);
    if (!interp->call(hashFunc, 1, result))
        throw PythonException(result);

    if (!result.isInt()) {
        result = gc.create<TypeError>("__hash__ method should return an int");
        throw PythonException(result);
    }

    return result.toInt();
}

bool Dict::ValuesEqual::operator()(Value a, Value b) const
{
    Stack<Value> result;
    Stack<Value> eqFunc;
    if (!a.maybeGetAttr(Name::__eq__, eqFunc)) {
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

    return obj->as<Boolean>()->value();
}
