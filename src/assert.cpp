#include "assert.h"

#include <cstdio>
#include <cstdlib>

void crash(const char* message)
{
    if (message)
        fprintf(stderr, "Abort: %s", message);
    fflush(nullptr);
    int* ptr = nullptr;
    *ptr = 0;
    exit(1);
}

void printAssertMessage(const char* cond, const char* file, unsigned line)
{
    fprintf(stderr, "%s:%d: assertion failed:\n  %s\n", file, line, cond);
}
