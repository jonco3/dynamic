#include "common.h"

#include "block.h"
#include "interp.h"
#include "input.h"

#include "sysexits.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>

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

static string readFile(string filename)
{
    ifstream file(filename);
    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static bool runTopLevel(string text, string filename) {
    Root<Block*> block(Block::buildTopLevel(Input(text, filename)));
    Value result;
    Interpreter interp;
    bool ok = interp.interpret(block, result);
    if (!ok)
        cout << "Error" << endl;
    else
        cout << repr(result) << endl;
    return ok;
}

static int runRepl()
{
    char* line;
    while (line = readOneLine(), line != NULL)
        runTopLevel(line, "<none>");
    cout << endl;
    return EX_OK;
}

static int runProgram(const char* filename, int arg_count, const char* args[])
{
    //todo: args
    //RootVector<String> argStrings(arg_count);
    //for (unsigned i = 0 ; i < arg_count ; ++i)
    //    argStrings[i] = new String(args[i]);
    if (!runTopLevel(readFile(filename), filename))
        return EX_SOFTWARE;

    return EX_OK;
}

static int runExprs(int count, const char* args[])
    {
    for (unsigned i = 0 ; i < count ; ++i) {
        // todo: should probably parse thse as expressions
        if (!runTopLevel(args[i], "<none>"))
            return EX_SOFTWARE;
    }

    return EX_OK;
    }

const char* usageMessage[] =
    {
    "usage:",
    "",
    "  dynamic            -- enter the REPL",
    "  dynamic FILE ARG*  -- run script from file",
    "  dynamic -e EXPRS   -- execute expressions from commandline",
    NULL
    };

static void badUsage()
    {
    const char** ptr = usageMessage;
    while (*ptr)
        cerr << *ptr << endl;
    exit(EX_USAGE);
    }

int main(int argc, const char* argv[])
    {
    int r = 0;
    int pos = 1;

    bool expr = false;

    while ((argc - pos) > 0 && argv[pos][0] == '-') {
        const char* opt = argv[pos];
        ++pos;
        if (strcmp("-e", opt) == 0)
            expr = true;
        else
            badUsage();
    }

    init();

    if (expr)
        r = runExprs(argc - pos, &argv[pos]);
    else if ((argc - pos) == 0)
        r = runRepl();
    else
        r = runProgram(argv[pos], argc - pos, &argv[pos]);

    final();

    return r;
    }
