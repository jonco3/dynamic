#ifndef __INPUT_H__
#define __INPUT_H__

#include <string>

using namespace std;

struct Input
{
    Input(string text, string file = "") : text(text), file(file) {}
    Input(const char* text) : text(text), file("") {}

    const string text;
    const string file;
};

#endif
