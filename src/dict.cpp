#include "dict.h"

#include "bool.h"
#include "callable.h"
#include "exception.h"
#include "repr.h"

GlobalRoot<Class*> Dict::ObjectClass;

static bool dict_contains(Traced<Value> arg1, Traced<Value> arg2,
                          Root<Value>& resultOut) {
    Dict* dict = arg1.toObject()->as<Dict>();
    return dict->contains(arg2, resultOut);
}

static bool dict_getitem(Traced<Value> arg1, Traced<Value> arg2,
                          Root<Value>& resultOut) {
    Dict* dict = arg1.toObject()->as<Dict>();
    return dict->getitem(arg2, resultOut);
}

static bool dict_setitem(Traced<Value> arg1, Traced<Value> arg2,
                         Traced<Value> arg3, Root<Value>& resultOut) {
    Dict* dict = arg1.toObject()->as<Dict>();
    return dict->setitem(arg2, arg3, resultOut);
}

void Dict::init()
{
    Root<Class*> cls(gc::create<Class>("dict"));
    Root<Value> value;
    value = gc::create<Native2>(dict_contains);  cls->setAttr("__contains__", value);
    value = gc::create<Native2>(dict_getitem);   cls->setAttr("__getitem__", value);
    value = gc::create<Native3>(dict_setitem);   cls->setAttr("__setitem__", value);
    ObjectClass.init(cls);
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
    for (auto i = entries_.begin(); i != entries_.end(); ++i) {
        Value key = i->first;
#ifdef DEBUG
        Value prior = key;
#endif
        gc::trace(t, &key);
        assert(key == prior);
        gc::trace(t, &i->second);
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
        resultOut = gc::create<Exception>("KeyError", repr(key));
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
