#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

#include "class.h"

struct Exception : public Object
{
    static void init();
    static GlobalRoot<Class*> ObjectClass;

    Exception(const string& className, const string& message)
      : Object(ObjectClass), className_(className), message_(message) {}

    string fullMessage() const;
    string message() { return message_; }

    virtual void print(ostream& os) const;

  private:
    string className_;
    string message_;
};

#endif
