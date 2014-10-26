#ifndef __COMMON_H__
#define __COMMON_H__

#include <string>

using namespace std;

extern void init();
extern void final();
extern string readFile(string filename);
extern bool runModule(string text, string filename);
extern bool runStatements(string text, string filename);
#endif
