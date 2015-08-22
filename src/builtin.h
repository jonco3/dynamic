#ifndef __BUILTIN_H__
#define __BUILTIN_H__

#include "object.h"

extern GlobalRoot<Object*> Builtin;

extern GlobalRoot<Class*> SequenceIterator;

extern void initBuiltins(const string& libDir);

#endif
