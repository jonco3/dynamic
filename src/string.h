#ifndef __STRING_H__
#define __STRING_H__

#include "object.h"

#include <string>

struct String : public Object
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    static GlobalRoot<String*> EmptyString;

    static Value get(const string& v);

    const string& value() { return value_; }
    virtual void print(ostream& os) const;

    String(const string& v);

  private:
    string value_;
};

#endif
