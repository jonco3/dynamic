#ifndef __COMMON_H__
#define __COMMON_H__

#include "value.h"

#include <string>

using namespace std;

extern void init1();
extern void init2();
extern void final();
extern string readFile(string filename);
extern void printException(Value value);
extern bool runModule(string text, string filename, Value* valueOut = nullptr,
                      Object* global = nullptr);
#endif
