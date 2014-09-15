#ifndef __UTILITY_H__
#define __UTILITY_H__

#include <sstream>

template <typename T>
std::string repr(const T& t)
{
    std::ostringstream s;
    s << t;
    return s.str();
}

#endif
