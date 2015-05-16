#ifndef __RERP_H__
#define __RERP_H__

#include <sstream>
#include <iostream>

using namespace std;

template <typename T>
inline string repr(const T& t)
{
    ostringstream s;
    s << t;
    return s.str();
}

#endif
