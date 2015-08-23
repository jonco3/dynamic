#ifndef __BUILTIN_H__
#define __BUILTIN_H__

#include "object.h"

struct Function;

extern GlobalRoot<Object*> Builtin;

// Self-hosted classes and functions.
extern GlobalRoot<Class*> SequenceIterator;
extern GlobalRoot<Function*> IterableToList;

extern void initBuiltins(const string& libDir);

#endif
