#ifndef __NAME_H__
#define __NAME_H__

#include "specials.h"

#include "assert.h"

/*
 * Currently just use std::sring for names, but this could be changed to
 * something more lightweight.
 */

#include <string>

using namespace std;

#define for_each_predeclared_name(name)                                       \
    name(__get__)                                                             \
    name(__set__)                                                             \
    name(__delete__)                                                          \
    name(__class__)                                                           \
    name(__name__)                                                            \
    name(__bases__)                                                           \
    name(__builtins__)                                                        \
    name(__contains__)                                                        \
    name(__len__)                                                             \
    name(__new__)                                                             \
    name(__init__)                                                            \
    name(__call__)                                                            \
    name(__hash__)                                                            \
    name(__eq__)                                                              \
    name(__pos__)                                                             \
    name(__neg__)                                                             \
    name(__invert__)                                                          \
    name(__setitem__)                                                         \
    name(__delitem__)                                                         \
    name(__getitem__)                                                         \
    name(__iter__)                                                            \
    name(__next__)                                                            \
    name(__str__)                                                             \
    name(__repr__)

struct Name
{
#define declare_name(name)                                                    \
    static Name name;
    for_each_predeclared_name(declare_name)
#undef declare_name

    static Name binMethod[CountBinaryOp];
    static Name binMethodReflected[CountBinaryOp];
    static Name augAssignMethod[CountBinaryOp];
    static Name compareMethod[CountCompareOp];
    static Name compareMethodReflected[CountCompareOp];
    static Name listCompResult;

    Name()
      : string_(nullptr) {}

    Name(const string& str)
      : string_(intern(str)) {}

    Name(const char* str)
      : string_(str ? intern(str) : nullptr) {}

    bool isNull() const {
        return string_ == nullptr;
    }

    const string& get() const {
        assert(!isNull());
        return *string_;
    }

    operator const string&() const { return get(); }

    bool operator==(const Name& other) const {
        return string_ == other.string_;
    }
    bool operator!=(const Name& other) const {
        return string_ != other.string_;
    }

    bool operator==(const string& other) const {
        return get() == other;
    }
    bool operator!=(const string& other) const {
        return get() != other;
    }

    bool operator==(const char* other) const {
        return get() == other;
    }
    bool operator!=(const char* other) const {
        return get() != other;
    }

    size_t asBits() { return size_t(string_); }

  private:
    const string* string_;

    static string* intern(const string& s);
};

inline ostream& operator<<(ostream& s, Name name)
{
    s << name.get();
    return s;
}

inline string operator+(const string& a, Name b) {
    return a + b.get();
}

namespace std {
template<>
struct hash<Name>
{
    typedef Name argument_type;
    typedef std::size_t result_type;

    size_t operator()(Name n) const
    {
        return n.asBits();
    }
};
} /* namespace std */

extern void initNames();
extern void shutdownNames();

#endif
