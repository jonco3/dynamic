#include "name.h"

#include "string.h"

#include <unordered_map>
#include <iostream>
#include <string>

struct InternedStringMap : public Cell
{
    void traceChildren(Tracer& t) override;
    InternedString* get(const string& s);
    unordered_map<string, Heap<InternedString*>> strings_;
};

static GlobalRoot<InternedStringMap*> InternedStrings;

#define define_name(name)                                                     \
    Name Names::name;
for_each_predeclared_name(define_name)
#undef define_name

Name Names::binMethod[CountBinaryOp];
Name Names::binMethodReflected[CountBinaryOp];
Name Names::augAssignMethod[CountBinaryOp];
Name Names::compareMethod[CountCompareOp];
Name Names::compareMethodReflected[CountCompareOp];
Name Names::listCompResult;
Name Names::evalFuncName;
Name Names::execFuncName;

vector<Name> EmptyNameVector;

Name internString(const string& s)
{
    return InternedStrings->get(s);
}

void initNames()
{
    assert(!InternedStrings); // Stop Name being used before map initialised.
    assert(String::ObjectClass);

    InternedStrings.init(gc.create<InternedStringMap>());

#define init_name(name)                                                       \
    Names::name = internString(#name);

    for_each_predeclared_name(init_name)

#undef init_name

#define init_name(name, token, method, rmethod, imethod)                      \
    Names::binMethod[Binary##name] = internString(method);                    \
    Names::binMethodReflected[Binary##name] = internString(rmethod);          \
    Names::augAssignMethod[Binary##name] = internString(imethod);

    for_each_binary_op(init_name)

#undef init_name

#define init_name(name, token, method, rmethod)                               \
    Names::compareMethod[Compare##name] = internString(method);               \
    Names::compareMethodReflected[Compare##name] = internString(rmethod);

    for_each_compare_op(init_name)

#undef init_name

    Names::listCompResult = internString("%result");
    Names::evalFuncName = internString("<eval>");
    Names::execFuncName = internString("<exec>");
}

void InternedStringMap::traceChildren(Tracer& t)
{
    for (auto i : strings_) {
        String* s = i.second;
        assert(s->type());
        gc.trace(t, &i.second);
    }
}

InternedString* InternedStringMap::get(const string& s)
{
    assert(s != "");

    auto i = strings_.find(s);
    if (i != strings_.end()) {
        assert(i->second->value() == s);
        String* s = i->second;
        assert(s->type());
        return i->second;
    }

    Stack<InternedString*> interned;
    interned = static_cast<InternedString*>(String::get(s));
    assert(interned->type());
    strings_[s] = interned;
    assert(strings_.find(s) != strings_.end());
    return interned;
}

bool isSpecialName(Name name)
{
    const string& s = name->value();
    return s.size() > 4 &&
           s.substr(0, 2) == "__" && s.substr(s.size() - 2, 2) == "__";
}

ostream& operator<<(ostream& s, const Name str)
{
    s << str->value();
    return s;
}

string operator+(const string& a, const Name b)
{
    return a + b->value();
}
