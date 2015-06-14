#include "name.h"

#include "assert.h"

#include <unordered_map>
#include <iostream>
#include <string>

static bool namesInitialised = false;
static unordered_map<string, string*> internedNames;

void initNames()
{
    assert(!namesInitialised);
    namesInitialised = true;
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
