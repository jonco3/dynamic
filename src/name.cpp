#include "name.h"

#include "string.h"

#include <unordered_map>
#include <iostream>
#include <string>

struct InternedString : public String
{
    static InternedString* get(const string& str);

    InternedString() = delete;
};

struct InternedStringMap : public Cell
{
    void traceChildren(Tracer& t) override;
    InternedString* get(const string& s);
    unordered_map<string, Heap<InternedString*>> strings_;
};

static GlobalRoot<InternedStringMap*> InternedStrings;

#define define_name(name)                                                     \
    Name Name::name;
for_each_predeclared_name(define_name)
#undef define_name

Name Name::binMethod[CountBinaryOp];
Name Name::binMethodReflected[CountBinaryOp];
Name Name::augAssignMethod[CountBinaryOp];
Name Name::compareMethod[CountCompareOp];
Name Name::compareMethodReflected[CountCompareOp];
Name Name::listCompResult;

void initNames()
{
    assert(!InternedStrings); // Stop Name being used before map initialised.
    InternedStrings.init(gc.create<InternedStringMap>());

#define init_name(name)                                                       \
    Name::name = Name(#name);

    for_each_predeclared_name(init_name)

#undef init_name

#define init_name(name, token, method, rmethod, imethod)                      \
    Name::binMethod[Binary##name] = Name(method);                             \
    Name::binMethodReflected[Binary##name] = Name(rmethod);                   \
    Name::augAssignMethod[Binary##name] = Name(imethod);

    for_each_binary_op(init_name)

#undef init_name

#define init_name(name, token, method, rmethod)                               \
    Name::compareMethod[Compare##name] = Name(method);                        \
    Name::compareMethodReflected[Compare##name] = Name(rmethod);

    for_each_compare_op(init_name)

#undef init_name

    Name::listCompResult = Name("%result");
}

const string& Name::get() const
{
    assert(!isNull());
    return string_->value();
}

void InternedStringMap::traceChildren(Tracer& t)
{
    for (auto i : strings_)
        gc.trace(t, &i.second);
}

InternedString* InternedStringMap::get(const string& s)
{
    auto i = strings_.find(s);
    if (i != strings_.end()) {
        assert(i->second->value() == s);
        return i->second;
    }

    Stack<InternedString*> interned;
    interned = static_cast<InternedString*>(String::get(s));
    strings_[s] = interned;
    assert(strings_.find(s) != strings_.end());
    return interned;
}

InternedString* Name::intern(const string& s)
{
    return InternedStrings->get(s);
}
