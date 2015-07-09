#ifndef __COMPILER_H__
#define __COMPILER_H__

#include "block.h"
#include "input.h"
#include "object.h"

extern void CompileModule(const Input& input, Traced<Object*> globalsArg,
                          MutableTraced<Block*> blockOut);

#endif
