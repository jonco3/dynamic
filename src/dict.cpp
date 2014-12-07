#include "dict.h"

#include "callable.h"
#include "exception.h"

GlobalRoot<Class*> Dict::ObjectClass;

struct DictClass : public Class
{
    DictClass() : Class("dict") {}
};

void Dict::init()
{
    Root<DictClass*> cls(new DictClass);
    ObjectClass.init(cls);
}

Dict::Dict(unsigned size, const Value* values)
  : Object(ObjectClass), entries_(size)
{
    for (unsigned i = 0; i < size; ++i)
        entries_[values[i * 2]] = values[i * 2 + 1];
}

void Dict::traceChildren(Tracer& t)
{
    for (auto i = entries_.begin(); i != entries_.end(); ++i) {
        Value key = i->first;
        Value prior = key;
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
}

#endif
