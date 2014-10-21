#include "common.h"

#include "bool.h"
#include "callable.h"
#include "class.h"
#include "frame.h"
#include "integer.h"
#include "none.h"
#include "object.h"
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
