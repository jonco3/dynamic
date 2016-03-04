#include "test.h"

#include "assert.h"

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
