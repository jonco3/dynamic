#ifndef __TEST_H__
#define __TEST_H__

#include <iostream>

#include <cstdlib>

typedef void (*TestFunc)();

struct TestCase
{
    TestCase(const char* name, TestFunc body);
    const char *name;
    TestFunc body;
};

#define CPP_CONCAT_(a, b) a##b
#define CPP_CONCAT(a, b) CPP_CONCAT_(a, b)

#define testcase(name, body)                                                           \
    static void CPP_CONCAT(testcase_body_, __LINE__)()                                 \
        body                                                                           \
    static TestCase CPP_CONCAT(testcase_obj_, __LINE__)(name, CPP_CONCAT(testcase_body_, __LINE__))

extern void runTests();

template <typename A>
inline void testFailure(const char* testStr,
                        const char* actualStr, const char* expectedStr,
                        const A actual, const char* file, unsigned line)
{
    using namespace std;
    cerr << file << ":" << line << ": test failed: ";
    cerr << actualStr << " " << testStr << " " << expectedStr;
    cerr << " but got " << actual << " at " << file << " line " << line << endl;
    flush(cerr);
    exit(1);
}

template <typename A, typename E>
inline void testEqualImpl(const A actual, const E expected,
                          const char* actualStr, const char* expectedStr,
                          const char* file, unsigned line)
{
    if (actual != expected)
        testFailure("==", actualStr, expectedStr, actual, file, line);
}

template <>
inline void testEqualImpl(const char* actual, const char* expected,
                          const char* actualStr, const char* expectedStr,
                          const char* file, unsigned line)
{
    if (strcmp(actual, expected) != 0)
        testFailure("==", actualStr, expectedStr, actual, file, line);
}

#define testEqual(actual, expected)                                           \
    testEqualImpl(actual, expected, #actual, #expected, __FILE__, __LINE__)

inline void testTrueImpl(bool actual, const char* actualStr,
                         const char* file, unsigned line)
{
    if (!actual)
        testFailure("==", actualStr, "true", actual, file, line);
}

#define testTrue(actual)                                                      \
    testTrueImpl(actual, #actual, __FILE__, __LINE__)

#endif
