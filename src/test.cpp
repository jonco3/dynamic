#include "test.h"

#include <vector>

std::vector<TestCase> testcases;

TestCase::TestCase(const char* name, TestFunc body) :
  name(name),
  body(body)
{
    testcases.push_back(*this);
}

void runTests()
{
    printf("Running tests\n");
    for (auto i = testcases.begin(); i != testcases.end(); ++i) {
        const TestCase& t = *i;
        printf("  %s\n", t.name);
        t.body();
    }
    printf("All tests passed.\n");
}
