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

    virtual bool equals(Object* other) const override {
        return other->is<String>() && value_ == other->as<String>()->value_;
    }

    virtual size_t hash() const override {
        return std::hash<string>()(value_);
    }

    String(const string& v);

  private:
    string value_;
};

#endif
