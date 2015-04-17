#ifndef __COMMON_H__
#define __COMMON_H__

#include "value.h"

#include <string>

using namespace std;

extern bool debugMode;

extern void init1();
extern void init2(const string& libDir);
extern void final();
extern string readFile(string filename);
extern void printException(Value value);
extern bool runModule(string text, string filename, Traced<Object*> global,
                      Value* valueOut = nullptr);
#endif
