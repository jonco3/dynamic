#ifndef __INPUT_H__
#define __INPUT_H__

#include <string>

struct Input
{
    Input(std::string text, std::string file = "") : text(text), file(file) {}
    Input(const char* text) : text(text), file("") {}

    const std::string text;
    const std::string file;
};

#endif
