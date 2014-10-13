#ifndef __NONE_H__
#define __NONE_H__

#include "object.h"

extern GlobalRoot<Object*> None;
extern GlobalRoot<Object*> UninitializedSlot;
extern void initSingletons();

#endif
