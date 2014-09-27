#ifndef __UTILITY_H__
#define __UTILITY_H__

#include <sstream>

using namespace std;

template <typename T>
string repr(const T& t)
{
    ostringstream s;
    s << t;
    return s.str();
}

#endif
