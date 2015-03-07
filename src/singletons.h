#ifndef __SINGLETONS_H__
#define __SINGLETONS_H__

#include "object.h"

extern GlobalRoot<Object*> None;
extern GlobalRoot<Object*> NotImplemented;
extern GlobalRoot<Object*> UninitializedSlot;
extern void initSingletons();

#endif
