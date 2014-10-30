#include "exception.h"

GlobalRoot<Class*> Exception::ObjectClass;

void Exception::init()
{
    ObjectClass.init(new Class("Exception"));  // todo: whatever this is really called
}
