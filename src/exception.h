#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

#include "common.h"
#include "object.h"
#include "string.h"
#include "token.h"

struct InstrThunk;

struct Exception : public Object
{
    static void init();
    static GlobalRoot<Class*> ObjectClass;

    Exception(Traced<Class*> cls, const string& message);
    Exception(Traced<Class*> cls, Traced<String*> message);

    bool hasTraceback() const { return !traceback_.empty(); }
    void recordTraceback(const InstrThunk *instrp);

    string className() const;
    string message() const;

    string fullMessage() const;
    string traceback() const;

    void print(ostream& s) const override;

  private:
    vector<TokenPos> traceback_;

    void init(Traced<String*> message);
};

#define for_each_exception_class(cls)                                         \
    cls(AssertionError)                                                       \
    cls(AttributeError)                                                       \
    cls(ImportError)                                                          \
    cls(IndexError)                                                           \
    cls(KeyError)                                                             \
    cls(MemoryError)                                                          \
    cls(NameError)                                                            \
    cls(NotImplementedError)                                                  \
    cls(OSError)                                                              \
    cls(RuntimeError)                                                         \
    cls(StopIteration)                                                        \
    cls(SyntaxError)                                                          \
    cls(TypeError)                                                            \
    cls(UnboundLocalError)                                                    \
    cls(ValueError)

#define declare_exception_class(name)                                         \
    struct name : public Exception                                            \
    {                                                                         \
        static GlobalRoot<Class*> ObjectClass;                                \
                                                                              \
        name(const string& message = "")                                      \
          : Exception(ObjectClass, message)                                   \
        {}                                                                    \
                                                                              \
        name(Traced<Class*> cls, const string& message)                       \
          : Exception(cls, message)                                           \
        {                                                                     \
            assert(cls->isDerivedFrom(ObjectClass));                          \
        }                                                                     \
    };

for_each_exception_class(declare_exception_class)
#undef define_exception_class

extern GlobalRoot<StopIteration*> StopIterationException;

// Wrap a python error in a C++ exception to propagate it out of STL machinery.
struct PythonException : public runtime_error
{
    PythonException(Value result)
      : runtime_error("python exception"), result_(result)
    {}

    Value result() const { return result_; }

  private:
    Value result_;
    AutoAssertNoGC nogc_;
};

template <typename T>
bool Raise(const string& message, MutableTraced<Value> resultOut)
{
    resultOut = gc.create<T>(message);
    return false;
}

template <typename T>
bool Raise(const char* message, MutableTraced<Value> resultOut)
{
    resultOut = gc.create<T>(message);
    return false;
}

template <typename T>
void ThrowException(const string& message)
{
    Stack<Value> error(gc.create<T>(message));
    throw PythonException(error);
}

#endif
