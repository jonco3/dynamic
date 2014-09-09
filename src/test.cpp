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
    try {
        for (auto i = testcases.begin(); i != testcases.end(); ++i) {
            const TestCase& t = *i;
            printf("  %s\n", t.name);
            t.body();
        }
        printf("All tests passed.\n");
    } catch (const std::runtime_error& err) {
        std::cerr << "Runtime error: " << err.what() << std::endl;
    }
}
