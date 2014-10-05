#include "test.h"

#include <vector>
#include <cassert>
#include <cstdio>

bool testExpectingException = false;

static vector<TestCase> testcases;
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
    assert(false);
    exit(1);
}

void runTests()
{
    runningTests = true;
    printf("Running tests\n");
    try {
        for (vector<TestCase>::iterator i = testcases.begin(); i != testcases.end(); ++i) {
            const TestCase& t = *i;
            printf("  %s\n", t.name);
            t.body();
        }
        printf("All tests passed.\n");
    } catch (const runtime_error& err) {
        cerr << "Runtime error: " << err.what() << endl;
        assert(false);
        exit(1);
    }
    runningTests = false;
}

void maybeAbortTests(const runtime_error& exception)
{
    if (runningTests && !testExpectingException)
    {
        cerr << "Exception thrown in test: " << exception.what() << endl;
        assert(false);
    }
}
