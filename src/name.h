#ifndef __NAME_H__
#define __NAME_H__

#include "specials.h"

#include "assert.h"

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
    name(__repr__)                                                            \
    name(self)                                                                \
    name(sys)                                                                 \
    name(argv)                                                                \
    name(message)                                                             \
    name(iterable)                                                            \
    name(start)                                                               \
    name(stop)                                                                \
    name(slice)                                                               \
    name(iter)                                                                \
    name(SequenceIterator)                                                    \
    name(iterableToList)                                                      \

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
} /* namespace Names */

extern Name internString(const string& str);

extern void initNames();
extern void shutdownNames();

extern ostream& operator<<(ostream& s, const Name str);
extern string operator+(const string& a, const Name b);

#endif
