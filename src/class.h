#ifndef __CLASS_H__
#define __CLASS_H__

#include "object.h"

#include <vector>

struct Class : public Object
{
    static void init();

    static Class* ObjectClass;

    Class(string name);

    const string& name() const { return name_; }

  private:
    string name_;
};

#endif
