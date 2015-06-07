#include "test.h"

#include "common.h"
#include "interp.h"

#include "sysexits.h"

#include <cassert>
#include <cstdio>
#include <cstring>

bool testExpectingException = false;

// I would have used a vector, but that would depend on it being initialized
// before the global TestCase objects.
static TestCase* testcasesHead = nullptr;
static TestCase* testcasesTail = nullptr;

static bool runningTests = false;

TestCase::TestCase(const char* name, TestFunc body) :
  name(name),
  body(body),
  next(nullptr)
{
    assert((testcasesHead != nullptr) == (testcasesTail != nullptr));
    if (testcasesHead)
        testcasesTail->next = this;
    else
        testcasesHead = this;
    testcasesTail = this;
}

void TestCase::run()
{
    printf("  %s\n", name);
    body();
}

void abortTests()
{
    fflush(stderr);
    assert(false);
    exit(1);
}

void TestCase::runAllTests()
{
    runningTests = true;
    printf("Running tests\n");
    try {
        for (TestCase* test = testcasesHead; test; test = test->next) {
            test->run();
        }
        printf("All tests passed.\n");
    } catch (const runtime_error& err) {
        cerr << "Runtime error: " << err.what() << endl;
        assert(false);
        exit(1);
    }
    runningTests = false;
}

void maybeAbortTests(string what)
{
    if (runningTests && !testExpectingException)
    {
        cerr << "Exception thrown in test: " << what << endl;
        assert(false);
    }
}

const char* usageMessage =
    "usage:\n"
    "  tests              -- enter the unit tests\n"
    "options:\n"
    "  -l LIBDIR          -- set directory to load libraries from\n"
#ifdef DEBUG
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

    string libDir = ".";

    while (pos != argc && argv[pos][0] == '-') {
        const char* opt = argv[pos++];
        if (strcmp("-l", opt) == 0 && pos != argc)
            libDir = argv[pos++];
#ifdef DEBUG
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
    init2(libDir);
    TestCase::runAllTests();
    final();
    return 0;
}
