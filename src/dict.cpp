#include "dict.h"

#include "bool.h"
#include "callable.h"
#include "exception.h"
#include "list.h"
#include "repr.h"

#include <stdexcept>

GlobalRoot<Class*> Dict::ObjectClass;

static bool dict_len(TracedVector<Value> args, Root<Value>& resultOut)
{
    Dict* dict = args[0].toObject()->as<Dict>();
    return dict->len(resultOut);
}

static bool dict_contains(TracedVector<Value> args, Root<Value>& resultOut)
{
    Dict* dict = args[0].toObject()->as<Dict>();
    return dict->contains(args[1], resultOut);
}

static bool dict_getitem(TracedVector<Value> args, Root<Value>& resultOut)
{
    Dict* dict = args[0].toObject()->as<Dict>();
    return dict->getitem(args[1], resultOut);
}

static bool dict_setitem(TracedVector<Value> args, Root<Value>& resultOut)
{
    Dict* dict = args[0].toObject()->as<Dict>();
    return dict->setitem(args[1], args[2], resultOut);
}

static bool dict_delitem(TracedVector<Value> args, Root<Value>& resultOut)
{
    Dict* dict = args[0].toObject()->as<Dict>();
    return dict->delitem(args[1], resultOut);
}

static bool dict_keys(TracedVector<Value> args, Root<Value>& resultOut)
{
    Dict* dict = args[0].toObject()->as<Dict>();
    return dict->keys(resultOut);
}

void Dict::init()
{
    ObjectClass.init(gc.create<Class>("dict"));
    initNativeMethod(ObjectClass, "__len__", dict_len, 1);
    initNativeMethod(ObjectClass, "__contains__", dict_contains, 2);
    initNativeMethod(ObjectClass, "__getitem__", dict_getitem, 2);
    initNativeMethod(ObjectClass, "__setitem__", dict_setitem, 3);
    initNativeMethod(ObjectClass, "__delitem__", dict_delitem, 2);
    initNativeMethod(ObjectClass, "keys", dict_keys, 1);
    // __eq__ and __ne__ are supplied by lib/internal.py
}

Dict::Dict(const TracedVector<Value>& values)
  : Object(ObjectClass), entries_(values.size() / 2)
{
    unsigned entryCount = values.size() / 2;
    for (unsigned i = 0; i < entryCount; ++i)
        entries_[values[i * 2]] = values[i * 2 + 1];
}

void Dict::traceChildren(Tracer& t)
{
    Object::traceChildren(t);
    for (auto i = entries_.begin(); i != entries_.end(); ++i) {
        Value key = i->first;
#ifdef DEBUG
        Value prior = key;
#endif
        gc.trace(t, &key);
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

bool Dict::len(Root<Value>& resultOut)
{
    resultOut = Integer::get(entries_.size());
    return true;
}

bool Dict::contains(Traced<Value> key, Root<Value>& resultOut)
{
    bool found = entries_.find(key) != entries_.end();
    resultOut = Boolean::get(found);
    return true;
}

bool Dict::getitem(Traced<Value> key, Root<Value>& resultOut)
{
    auto i = entries_.find(key);
    if (i == entries_.end()) {
        resultOut = gc.create<KeyError>(repr(key));
        return false;
    }

    resultOut = i->second;
    return true;
}

bool Dict::setitem(Traced<Value> key, Traced<Value> value, Root<Value>& resultOut)
{
    entries_[key] = value;
    resultOut = value;
    return true;
}

bool Dict::delitem(Traced<Value> key, Root<Value>& resultOut)
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

bool Dict::keys(Root<Value>& resultOut)
{
    // todo: should be some kind of iterator?
    Root<Tuple*> keys(Tuple::createUninitialised(entries_.size()));
    size_t index = 0;
    for (const auto& i : entries_)
        keys->initElement(index++, i.first);
    resultOut = keys;
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
    Root<Value> result;
    Root<Value> hashFunc;
    if (!v.maybeGetAttr("__hash__", hashFunc)) {
        result = gc.create<TypeError>("Object has no __hash__ method");
        throw PythonException(result);
    }

    RootVector<Value> args(1);
    args[0] = v;
    if (!Interpreter::instance().call(hashFunc, args, result))
        throw PythonException(result);

    if (!result.isInt32()) {
        result = gc.create<TypeError>("__hash__ method should return an int");
        throw PythonException(result);
    }

    return result.asInt32();
}

bool Dict::ValuesEqual::operator()(Value a, Value b) const
{
    Root<Value> result;
    Root<Value> eqFunc;
    if (!a.maybeGetAttr("__eq__", eqFunc)) {
        result = gc.create<TypeError>("Object has no __eq__ method");
        throw PythonException(result);
    }

    RootVector<Value> args(2);
    args[0] = a;
    args[1] = b;
    if (!Interpreter::instance().call(eqFunc, args, result))
        throw PythonException(result);

    Root<Object*> obj(result.toObject());
    if (!obj->is<Boolean>()) {
        result = gc.create<TypeError>("__eq__ method should return a bool");
        throw PythonException(result);
    }

    return obj->as<Boolean>()->value();
}

#ifdef BUILD_TESTS

#include "test.h"
#include "interp.h"

testcase(dict)
{
    testInterp("{}", "{}");
    testInterp("{1: 2}", "{1: 2}");
    testInterp("{1: 2,}", "{1: 2}");
    testInterp("{'foo': 2 + 2}", "{'foo': 4}");
    testInterp("{2 + 2: 'bar'}", "{4: 'bar'}");
    testInterp("1 in {1: 2}", "True");
    testInterp("2 in {1: 2}", "False");

    testException("{}[0]", "KeyError: 0");
    testInterp("{'baz': 2}['baz']", "2");

    testInterp("{}[1] = 2", "2");
    testInterp("a = {}; a[1] = 2; a", "{1: 2}");
    testInterp("a = {1: 2}; a[1] = 3; a", "{1: 3}");
}

#endif
