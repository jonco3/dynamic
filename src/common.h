#ifndef __COMMON_H__
#define __COMMON_H__

#include "value.h"

#include <string>

using namespace std;

struct Env;

extern bool debugMode;

extern void crash(const char* message);

extern void init1();
extern void init2(const string& libDir);
extern void final();
extern string readFile(string filename);
extern void printException(Value value);
extern Env* createTopLevel();
extern bool runModule(string text, string filename, Traced<Env*> global,
                      MutableTraced<Value> resultOut);
extern bool runModule(string text, string filename, Traced<Env*> global);
#endif
