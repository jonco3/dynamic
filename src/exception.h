#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

#include "object.h"
#include "string.h"

struct Exception : public Object
{
    static void init();
    static GlobalRoot<Class*> ObjectClass;

    Exception(const string& className, const string& message);

    static bool create(TracedVector<Value> args, Root<Value>& resultOut);

    string fullMessage() const;

    string className() const;
    string message() const;

    virtual void print(ostream& os) const;

  protected:
    Exception(Traced<Class*> cls, const string& className,
              const string& message);

  private:
    void init(Traced<Value> className, Traced<Value> message);
};

struct StopIteration : public Exception
{
    static void init();
    static GlobalRoot<Class*> ObjectClass;

    StopIteration() : Exception(ObjectClass, "StopIteration", "") {}
};

bool checkInstanceOf(Traced<Value> v, Traced<Class*> cls, Root<Value>& resultOut);

#endif
