#include "common.h"

#include "block.h"
#include "interp.h"
#include "input.h"

#include "sysexits.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

#ifdef USE_READLINE

#define Function Readline_Function

#include <readline/readline.h>
#include <readline/history.h>

#undef Function

static char *lineRead = (char *)NULL;

char *readOneLine()
{
    if (lineRead) {
        free(lineRead);
        lineRead = (char *)NULL;
    }

    lineRead = readline("> ");

    if (lineRead && *lineRead)
        add_history(lineRead);

    return lineRead;
}

#else

const size_t MaxLineLength 256;
static char line_buffer[MaxLineLength];

char *readOneLine()
{
    printf("> ");
    char* result = fgets(line_buffer, MaxLineLength, stdin);
    return result;
}

#endif

static int runRepl()
{
    char* line;
    Root<Object*> globals(createTopLevel());
    while (line = readOneLine(), line != NULL) {
        Root<Value> result;
        bool ok = runModule(line, "<none>", globals, &result);
        if (ok)
            cout << result << endl;
    }
    cout << endl;
    return EX_OK;
}

static int runProgram(const char* filename, int arg_count, const char* args[])
{
    //todo: args
    //RootVector<String> argStrings(arg_count);
    //for (unsigned i = 0 ; i < arg_count ; ++i)
    //    argStrings[i] = gc.create<String>(args[i]);
    if (!runModule(readFile(filename), filename, None))
        return EX_SOFTWARE;

    return EX_OK;
}

static int runExprs(int count, const char* args[])
{
    for (int i = 0 ; i < count ; ++i) {
        // todo: should probably parse thse as expressions
        if (!runModule(args[i], "<none>", None))
            return EX_SOFTWARE;
    }

    return EX_OK;
}

const char* usageMessage =
    "usage:\n"
    "  dynamic            -- enter the REPL\n"
    "  dynamic FILE ARG*  -- run script from file\n"
    "  dynamic -e EXPRS   -- execute expressions from commandline\n"
    "options:\n"
    "  -l LIBDIR          -- set directory to load libraries from";

static void badUsage()
{
    cerr << usageMessage << endl;
    exit(EX_USAGE);
}

int main(int argc, const char* argv[])
{
    int r = 0;
    int pos = 1;

    bool expr = false;
    string libDir = ".";

    while (pos != argc && argv[pos][0] == '-') {
        const char* opt = argv[pos++];
        if (strcmp("-e", opt) == 0)
            expr = true;
        if (strcmp("-l", opt) == 0 && pos != argc)
            libDir = argv[pos++];
        else
            badUsage();
    }

    init1();
    init2(libDir);

    if (expr)
        r = runExprs(argc - pos, &argv[pos]);
    else if ((argc - pos) == 0)
        r = runRepl();
    else
        r = runProgram(argv[pos], argc - pos, &argv[pos]);

    final();

    return r;
}
