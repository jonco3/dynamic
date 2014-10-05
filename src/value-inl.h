#ifndef __VALUE_INL_H__
#define __VALUE_INL_H__

#include "value.h"

#include "object.h"

inline bool Value::isTrue() const
{
    return toObject()->isTrue();
}

#endif
