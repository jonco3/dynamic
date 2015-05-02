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

    static Object* createInstance(Traced<Class*> cls);
    Exception(Traced<Class*> cls);
    bool init(TracedVector<Value> args, Root<Value>& resultOut);

    void setPos(const TokenPos& pos);

    const TokenPos& pos() const { return pos_; }
    string className() const;
    string message() const;

    string fullMessage() const;

    virtual void print(ostream& os) const;

  private:
    TokenPos pos_;

    void init(Traced<Value> message);
};

#define for_each_exception_class(cls)                                         \
    cls(AssertionError)                                                       \
    cls(AttributeError)                                                       \
    cls(IndexError)                                                           \
    cls(KeyError)                                                             \
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
      name(const string& message = "")                                        \
          : Exception(ObjectClass, message)                                   \
        {}                                                                    \
    };
for_each_exception_class(declare_exception_class)
#undef define_exception_class

extern GlobalRoot<StopIteration*> StopIterationException;

bool checkInstanceOf(Traced<Value> v, Traced<Class*> cls, Root<Value>& resultOut);

#endif
