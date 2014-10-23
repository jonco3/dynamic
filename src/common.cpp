#include "common.h"

#include "bool.h"
#include "callable.h"
#include "class.h"
#include "frame.h"
#include "integer.h"
#include "object.h"
#include "singletons.h"
#include "test.h"

void init()
{
    Object::init();
    Class::init();
    Native::init();
    Function::init();
    Frame::init();
    Boolean::init();
    Integer::init();
    initSingletons();
}

void final()
{
    // todo
}
