#ifndef __EXCEPTION_H__
#define __EXCEPTION_H__

#include "class.h"

struct Exception : public Object
{
    static void init();
    static GlobalRoot<Class*> ObjectClass;

    Exception(const string& message) : Object(ObjectClass), message_(message) {}
    string message() { return message_; }

    virtual void print(ostream& os) const;

  private:
    string message_;
};

#endif
