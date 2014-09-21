#include "test.h"

#include <vector>
#include <cassert>

bool testExpectingException = false;

static std::vector<TestCase> testcases;
static bool runningTests = false;

TestCase::TestCase(const char* name, TestFunc body) :
  name(name),
  body(body)
{
    testcases.push_back(*this);
}

void abortTests()
{
    fflush(stderr);
    exit(1);
}

void runTests()
{
    runningTests = true;
    printf("Running tests\n");
    try {
        for (auto i = testcases.begin(); i != testcases.end(); ++i) {
            const TestCase& t = *i;
            printf("  %s\n", t.name);
            t.body();
        }
        printf("All tests passed.\n");
    } catch (const std::runtime_error& err) {
        std::cerr << "Runtime error: " << err.what() << std::endl;
        assert(false);
        exit(1);
    }
    runningTests = false;
}

void maybeAbortTests(const std::runtime_error& exception)
{
    if (runningTests && !testExpectingException)
    {
        std::cerr << "Exception thrown in test: " << exception.what() << std::endl;
        assert(false);
    }
}
