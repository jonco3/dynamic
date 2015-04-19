#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

#include "object.h"
#include "string.h"
#include "token.h"

struct Exception : public Object
{
    static void init();
    static GlobalRoot<Class*> ObjectClass;

    Exception(const string& className, const string& message);

    static bool create(TracedVector<Value> args, Root<Value>& resultOut);

    string className() const;
    string message() const;

    string fullMessage() const;

    virtual void print(ostream& os) const;

  protected:
    Exception(Traced<Class*> cls, const TokenPos& pos, const string& message);

  private:
    TokenPos pos_;

    void init(Traced<Value> className, Traced<Value> message);
};

struct StopIteration : public Exception
{
    static void init();
    static GlobalRoot<Class*> ObjectClass;

    static bool create(TracedVector<Value> args, Root<Value>& resultOut);

    StopIteration() : Exception(ObjectClass, TokenPos(), "") {}
};

struct TypeError : public Exception
{
    static void init();
    static GlobalRoot<Class*> ObjectClass;

    static bool create(TracedVector<Value> args, Root<Value>& resultOut);

    TypeError(string message) : Exception("TypeError", "message") {}
};

bool checkInstanceOf(Traced<Value> v, Traced<Class*> cls, Root<Value>& resultOut);

#endif
