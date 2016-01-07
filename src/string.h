#ifndef __STRING_H__
#define __STRING_H__

#include "object.h"

#include <string>

struct InternedString;

struct String : public Object
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;

    static GlobalRoot<String*> EmptyString;

    static String* get(const string& v);

    String(const string& v);
    String(Traced<Class*> cls);

    const string& value() const { return value_; }
    void print(ostream& s) const override;

    bool getitem(Traced<Value> index, MutableTraced<Value> resultOut);

  private:
    string value_;
};

struct InternedString : public String
{
    InternedString() = delete;
};

#endif
