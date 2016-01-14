#ifndef __INPUT_H__
#define __INPUT_H__

#include <string>

using namespace std;

struct Input
{
    Input(const string& text, const string& filename)
      : text(text), filename(filename)
    {}

    Input(const string& text)
      : text(text), filename("<unknown>")
    {}

    Input(const char* text)
      : text(text), filename("<unknown>")
    {}

    const string text;
    const string filename;
};

#endif
