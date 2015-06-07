#ifndef __STRING_H__
#define __STRING_H__

#include "object.h"

#include <string>

struct String : public Object
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    static GlobalRoot<String*> EmptyString;

    static String* get(const string& v);

    const string& value() { return value_; }
    void print(ostream& s) const override;

    String(const string& v);
    String(Traced<Class*> cls);

  private:
    string value_;
};

#endif
