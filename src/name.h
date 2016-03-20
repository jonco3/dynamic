#ifndef __NAME_H__
#define __NAME_H__

#include "specials.h"

#include "assert.h"

#include <string>

using namespace std;

#define for_each_predeclared_name(_)                                          \
    _(__get__)                                                                \
    _(__set__)                                                                \
    _(__delete__)                                                             \
    _(__class__)                                                              \
    _(__name__)                                                               \
    _(__bases__)                                                              \
    _(__builtins__)                                                           \
    _(__contains__)                                                           \
    _(__len__)                                                                \
    _(__new__)                                                                \
    _(__init__)                                                               \
    _(__call__)                                                               \
    _(__hash__)                                                               \
    _(__eq__)                                                                 \
    _(__pos__)                                                                \
    _(__neg__)                                                                \
    _(__invert__)                                                             \
    _(__setitem__)                                                            \
    _(__delitem__)                                                            \
    _(__getitem__)                                                            \
    _(__iter__)                                                               \
    _(__next__)                                                               \
    _(__str__)                                                                \
    _(__repr__)                                                               \
    _(self)                                                                   \
    _(sys)                                                                    \
    _(argv)                                                                   \
    _(message)                                                                \
    _(iterable)                                                               \
    _(start)                                                                  \
    _(stop)                                                                   \
    _(slice)                                                                  \
    _(iter)                                                                   \
    _(SequenceIterator)                                                       \
    _(iterableToList)                                                         \
    _(inUsingIteration)                                                       \
    _(inUsingSubscript)                                                       \
    _(__import__)                                                             \
    _(locals)                                                                 \
    _(globals)                                                                \
    _(__path__)                                                               \
    _(modules)                                                                \
    _(name)                                                                   \
    _(mode)                                                                   \
    _(closed)                                                                 \
    _(softSpace)                                                              \
    _(path)                                                                   \
    _(loadModule)                                                             \
    _(__package__)                                                            \
    _(__main__)                                                               \
    _(builtins)

struct InternedString;
struct String;

using Name = InternedString*;

#ifdef DEBUG
extern bool isSpecialName(Name name);
#endif

namespace std {
template<>
struct hash<Name>
{
    typedef Name argument_type;
    typedef std::size_t result_type;

    size_t operator()(Name n) const
    {
        return uintptr_t(n);
    }
};
} /* namespace std */

namespace Names
{
#define declare_name(name)                                                    \
    extern Name name;
    for_each_predeclared_name(declare_name)
#undef declare_name

    extern Name binMethod[CountBinaryOp];
    extern Name binMethodReflected[CountBinaryOp];
    extern Name augAssignMethod[CountBinaryOp];
    extern Name compareMethod[CountCompareOp];
    extern Name compareMethodReflected[CountCompareOp];
    extern Name listCompResult;
    extern Name evalFuncName;
    extern Name execFuncName;
} /* namespace Names */

extern Name internString(const string& str);

extern void initNames();
extern void shutdownNames();

extern ostream& operator<<(ostream& s, const Name str);
extern string operator+(const string& a, const Name b);

#endif
