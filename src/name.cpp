#include "name.h"

#include <unordered_map>
#include <iostream>
#include <string>

static bool namesInitialised = false;
static unordered_map<string, string*> internedNames;

#define define_name(name)                                                     \
    Name Name::name;
for_each_predeclared_name(define_name)
#undef define_name

Name Name::binMethod[CountBinaryOp];
Name Name::binMethodReflected[CountBinaryOp];
Name Name::augAssignMethod[CountBinaryOp];
Name Name::compareMethod[CountCompareOp];

void initNames()
{
    assert(!namesInitialised); // Stop Name being used before map initialised.
    namesInitialised = true;

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

#define init_name(name, token, method)                                        \
    Name::compareMethod[Compare##name] = Name(method);

    for_each_compare_op(init_name)

#undef init_name
}

void shutdownNames()
{
    assert(namesInitialised);
    for (const auto& i : internedNames)
        delete i.second;
    internedNames.clear();
    namesInitialised = false;
}

/* static */ string* Name::intern(const string& s)
{
    assert(namesInitialised);

    auto i = internedNames.find(s);
    if (i != internedNames.end()) {
        assert(*i->second == s);
        return i->second;
    }

    string* interned = new string(s);
    internedNames.emplace(s, interned);
    assert(internedNames.find(s) != internedNames.end());
    return interned;
}
