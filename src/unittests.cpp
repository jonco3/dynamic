#include "test.h"

#include "common.h"
#include "compiler.h"
#include "interp.h"

#include "sysexits.h"

// It's much faster to build all the tests in one go than compiling seperately
// and then linking.
#include "test.cpp"
#include "test/test_token.cpp"
#include "test/test_parser.cpp"
#include "test/test_gc.cpp"
#include "test/test_block.cpp"
#include "test/test_layout.cpp"
#include "test/test_object.cpp"
#include "test/test_interp.cpp"
#include "test/test_exception.cpp"
#include "test/test_numeric.cpp"
#include "test/test_string.cpp"
#include "test/test_list.cpp"
#include "test/test_dict.cpp"
#include "test/test_builtin.cpp"

const char* usageMessage =
    "usage:\n"
    "  tests              -- enter the unit tests\n"
    "options:\n"
    "  -i INTERNALSDIR    -- set directory to load intenals from\n"
#ifdef DEBUG
    "  -lc                -- log compiled bytecode\n"
    "  -le                -- log interpreter execution\n"
    "  -lf                -- log interpreter frames only\n"
    "  -lg                -- log GC activity\n"
#endif
    ;

static void badUsage()
{
    cerr << usageMessage << endl;
    exit(EX_USAGE);
}

int main(int argc, char *argv[])
{
    int pos = 1;

    string internalsDir = "internals";

    while (pos != argc && argv[pos][0] == '-') {
        const char* opt = argv[pos++];
        if (strcmp("-i", opt) == 0 && pos != argc)
            internalsDir = argv[pos++];
#ifdef DEBUG
        else if (strcmp("-lc", opt) == 0)
            logCompile = true;
        else if (strcmp("-le", opt) == 0)
            logFrames = logExecution = true;
        else if (strcmp("-lf", opt) == 0)
            logFrames = true;
        else if (strcmp("-lg", opt) == 0)
            logGC = true;
#endif
        else
            badUsage();
    }

    gc.minCollectAt = 10;
    gc.scheduleFactorPercent = 110;

    init1();
    init2(internalsDir);
    TestCase::runAllTests();
    final();
    return 0;
}
