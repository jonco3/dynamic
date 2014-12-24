#include "exception.h"

GlobalRoot<Class*> Exception::ObjectClass;

void Exception::init()
{
    ObjectClass.init(new Class("Exception"));  // todo: whatever this is really called
}

string Exception::fullMessage() const
{
    if (message_ == "")
        return className_;
    else
        return className_ + ": " + message_;
}

void Exception::print(ostream& os) const
{
    os << fullMessage();
}
