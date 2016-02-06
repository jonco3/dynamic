#include "common.h"

#include "block.h"
#include "dict.h"
#include "interp.h"
#include "input.h"
#include "list.h"
#include "string.h"

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

void maybeAbortTests(string what)
{}

static int runRepl()
{
    char* line;
    Stack<Env*> topLevel(createTopLevel());
    while (line = readOneLine(), line != NULL) {
        Stack<Value> result;
        bool ok = runModule(line, "<none>", topLevel, result);
        if (ok)
            cout << result << endl;
        else
            printException(result);
    }
    cout << endl;
    return EX_OK;
}

static int runProgram(const char* filename, int arg_count, const char* args[])
{
    Stack<Env*> topLevel(createTopLevel());
    RootVector<Value> argStrings(arg_count);
    for (int i = 0 ; i < arg_count ; ++i)
        argStrings[i] = gc.create<String>(args[i]);
    // todo: this is a hack until we can |import sys|
    Stack<Value> argv(gc.create<List>(argStrings));
    Stack<Value> sys(Object::create());
    sys.asObject()->setAttr(Names::argv, argv);
    topLevel->setAttr(Names::sys, sys);
    Stack<Value> main(gc.create<String>("__main__"));
    topLevel->setAttr(Names::__name__, main);
    if (!runModule(readFile(filename), filename, topLevel))
        return EX_SOFTWARE;

    return EX_OK;
}

static int runExprs(int count, const char* args[])
{
    Stack<Env*> globals;
    for (int i = 0 ; i < count ; ++i) {
        // todo: should probably parse thse as expressions
        if (!runModule(args[i], "<none>", globals))
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
    "  -l LIBDIR          -- set directory to load libraries from\n"
#ifdef LOG_EXECUTION
    "  -le                -- log interpreter execution\n"
    "  -lf                -- log interpreter frames only\n"
#endif
#ifdef DEBUG
    "  -lg                -- log GC activity\n"
    "  -lc                -- log compiled bytecode\n"
    "  -lb                -- log big integer arithmetic\n"
    "  -z N               -- perform GC every N allocations\n"
#endif
    "  -sg                -- log GC stats\n"
    ;

static void badUsage()
{
    cerr << usageMessage;
    exit(EX_USAGE);
}

int main(int argc, const char* argv[])
{
    int r = 0;
    int pos = 1;

    bool expr = false;
    string libDir = "lib";

    while (pos != argc && argv[pos][0] == '-') {
        const char* opt = argv[pos++];
        if (strcmp("-e", opt) == 0)
            expr = true;
        else if (strcmp("-l", opt) == 0 && pos != argc)
            libDir = argv[pos++];
#ifdef LOG_EXECUTION
        else if (strcmp("-le", opt) == 0)
            logFrames = logExecution = true;
        else if (strcmp("-lf", opt) == 0)
            logFrames = true;
#endif
#ifdef DEBUG
        else if (strcmp("-lg", opt) == 0)
            logGC = true;
        else if (strcmp("-lc", opt) == 0)
            logCompile = true;
        else if (strcmp("-lb", opt) == 0)
            logBigInt = true;
        else if (strcmp("-z", opt) == 0)
            gcZealPeriod = atol(argv[pos++]);
#endif
        else if (strcmp("-sg", opt) == 0)
            logGCStats = true;
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
