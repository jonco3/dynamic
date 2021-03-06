#ifndef __BUILTIN_H__
#define __BUILTIN_H__

#include "object.h"

struct Env;
struct Function;

extern GlobalRoot<Env*> Builtin;

// Self-hosted classes and functions.
extern GlobalRoot<Class*> SequenceIterator;
extern GlobalRoot<Function*> IterableToList;
extern GlobalRoot<Function*> InUsingIteration;
extern GlobalRoot<Function*> InUsingSubscript;
extern GlobalRoot<Function*> LoadModule;

extern bool builtinsInitialised;

extern void initBuiltins(const string& internalsPath);
extern void finalBuiltins();

#endif
