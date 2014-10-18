#ifndef __TEST_H__
#define __TEST_H__

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <functional>
#include <stdexcept>

using namespace std;

typedef void (*TestFunc)();

struct TestCase
{
    TestCase(const char* name, TestFunc body);
    void run();

    static void runAllTests();

  private:
    const char *name;
    TestFunc body;
    TestCase* next;
};

#define CPP_CONCAT_(a, b) a##b
#define CPP_CONCAT(a, b) CPP_CONCAT_(a, b)

#define testcase(name)                                                                 \
    static void CPP_CONCAT(testcase_body_, name)();                                          \
    static TestCase CPP_CONCAT(testcase_obj_, name)(#name, CPP_CONCAT(testcase_body_, name)); \
    static void CPP_CONCAT(testcase_body_, name)()

extern void abortTests();

template <typename A, typename E>
inline void testFailure(const char* testStr,
                        const char* actualStr, const char* expectedStr,
                        const A actual, const E expected,
                        const char* file, unsigned line)
{
    using namespace std;
    cerr << file << ":" << line << ": test failed: ";
    cerr << actualStr << " " << testStr << " " << expectedStr << endl;
    cerr << "  got: " << actual << " " << testStr << " " << expected << endl;
    cerr << "  at: " << file << " line " << line << endl;
    abortTests();
}

template <typename A, typename E>
inline void testEqualImpl(const A actual, const E expected,
                          const char* actualStr, const char* expectedStr,
                          const char* file, unsigned line)
{
    if (actual != expected)
        testFailure("==", actualStr, expectedStr, actual, expected, file, line);
}

template <>
inline void testEqualImpl(const char* actual, const char* expected,
                          const char* actualStr, const char* expectedStr,
                          const char* file, unsigned line)
{
    if (strcmp(actual, expected) != 0)
        testFailure("==", actualStr, expectedStr, actual, expected, file, line);
}

#define testEqual(actual, expected)                                           \
    testEqualImpl(actual, expected, #actual, #expected, __FILE__, __LINE__)

#define testTrue(actual)                                                      \
    testEqual(actual, true)

#define testFalse(actual)                                                     \
    testEqual(actual, false)

template <typename A>
inline void testThrowsFailureWrongException(const char* expectedName,
                                            const char* actualStr, const A& actual,
                                            const char* file, unsigned line)
{
    using namespace std;
    cerr << file << ":" << line << ": test failed: ";
    cerr << actualStr << " throws " << expectedName;
    cerr << " but threw " << actual << " at " << file << " line " << line << endl;
    abortTests();
}

inline void testThrowsFailureWrongException(const char* expectedName,
                                            const char* actualStr,
                                            const char* file, unsigned line)
{
    using namespace std;
    cerr << file << ":" << line << ": test failed: ";
    cerr << actualStr << " throws " << expectedName;
    cerr << " but threw something else at " << file << " line " << line << endl;
    abortTests();
}

inline void testThrowsFailureNoException(const char* expectedName, const char* actualStr,
                                         const char* file, unsigned line)
{
    using namespace std;
    cerr << file << ":" << line << ": test failed: ";
    cerr << actualStr << " throws " << expectedName;
    cerr << " but didn't throw at " << file << " line " << line << endl;
    abortTests();
}

extern bool testExpectingException;

#define testThrows(expr, exception)                                               \
    try {                                                                         \
        testExpectingException = true;                                            \
        expr;                                                                     \
        testThrowsFailureNoException(#exception, #expr, __FILE__, __LINE__);      \
    } catch (const exception& e) {                                                \
    } catch (...) {                                                               \
        testThrowsFailureWrongException(#exception, #expr, __FILE__, __LINE__);   \
    }                                                                             \
    testExpectingException = false

void maybeAbortTests(const runtime_error& exception);

#endif
