#include <cstdio>

#include "bool.h"
#include "callable.h"
#include "class.h"
#include "frame.h"
#include "integer.h"
#include "none.h"
#include "object.h"
#include "test.h"

int main(int argc, char *argv[])
{
    Object::init();
    Class::init();
    Native::init();
    Function::init();
    Frame::init();
    Boolean::init();
    Integer::init();
    initSingletons();

    runTests();
    return 0;
}
