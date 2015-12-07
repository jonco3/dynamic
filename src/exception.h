#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

#include "object.h"
#include "string.h"
#include "token.h"

struct Exception : public Object
{
    static void init();
    static GlobalRoot<Class*> ObjectClass;

    Exception(Traced<Class*> cls, const string& message);
    Exception(Traced<Class*> cls, Traced<String*> message);

    bool hasPos() const { return pos_.line != 0; }
    void setPos(const TokenPos& pos);

    const TokenPos& pos() const { return pos_; }
    string className() const;
    string message() const;

    string fullMessage() const;

    void print(ostream& s) const override;

  private:
    TokenPos pos_;

    void init(Traced<String*> message);
};

#define for_each_exception_class(cls)                                         \
    cls(AssertionError)                                                       \
    cls(AttributeError)                                                       \
    cls(IndexError)                                                           \
    cls(KeyError)                                                             \
    cls(MemoryError)                                                          \
    cls(NameError)                                                            \
    cls(NotImplementedError)                                                  \
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

#endif
