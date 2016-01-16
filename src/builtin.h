#ifndef __BUILTIN_H__
#define __BUILTIN_H__

#include "object.h"

struct Function;

extern GlobalRoot<Object*> Builtin;

// Self-hosted classes and functions.
extern GlobalRoot<Class*> SequenceIterator;
extern GlobalRoot<Function*> IterableToList;
extern GlobalRoot<Function*> InUsingIteration;
extern GlobalRoot<Function*> InUsingSubscript;

extern bool builtinsInitialised;

extern void initBuiltins(const string& libDir);
extern void finalBuiltins();

#endif
