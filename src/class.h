#ifndef __CLASS_H__
#define __CLASS_H__

#include "object.h"

#include <vector>

struct Class : public Object
{
    static Class ClassInstance;

    Class() : Object(&ClassInstance) {}
};

#endif
