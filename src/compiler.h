#ifndef __COMPILER_H__
#define __COMPILER_H__

#include "block.h"
#include "input.h"
#include "object.h"

// Compile source and return a CodeObject or an Exception.
extern bool CompileModule(const Input& input, Traced<Env*> globalsArg,
                          MutableTraced<Value> resultOut);

// Compile source and return a zero-argument Function or an Exception.
extern bool CompileEval(const Input& input, Traced<Env*> globalsArg,
                        Traced<Env*> localsArg,
                        MutableTraced<Value> resultOut);

// Compile source and return a zero-argument Function or an Exception.
extern bool CompileExec(const Input& input, Traced<Env*> globalsArg,
                        Traced<Env*> localsArg,
                        MutableTraced<Value> resultOut);

#endif
