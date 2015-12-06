#ifndef __UTILS_H__
#define __UTILS_H__

#include "assert.h"

#include <vector>

using namespace std;

#ifdef DEBUG
#define alwaysTrue(x) assert(x)
#else
#define alwaysTrue(x)                                                         \
    do {                                                                      \
        if (!x) {                                                             \
            fprintf(stderr, "Internal error\n");                              \
            exit(1);                                                          \
        }                                                                     \
    } while (false)
#endif

template <typename T>
struct AutoSetAndRestoreValue
{
    AutoSetAndRestoreValue(T& var, T newValue)
      : var_(var), prevValue_(var)
    {
        var_ = newValue;
    }

    ~AutoSetAndRestoreValue() {
        var_ = prevValue_;
    }

  private:
    T& var_;
    T prevValue_;
};

using AutoSetAndRestore = AutoSetAndRestoreValue<bool>;

template <typename T>
struct AutoPushStack
{
    AutoPushStack(vector<T>& stack, T value)
      : stack_(stack)
    {
        stack_.push_back(value);
#ifdef DEBUG
        value_ = value;
#endif
    }

    ~AutoPushStack() {
        assert(stack_.back() == value_);
        stack_.pop_back();
    }

  private:
    vector<T>& stack_;
#ifdef DEBUG
    T value_;
#endif
};

template <typename T>
inline bool contains(const vector<T>& v, T value)
{
    return find(v.begin(), v.end(), value) != v.end();
}

#endif
