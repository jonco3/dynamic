#include "src/exception.h"

#include "src/test.h"
#include "src/value-inl.h"

testcase(exception)
{
    Root<Exception*> exc;
    testExpectingException = true;
    exc = gc.create<Exception>(Exception::ObjectClass, "bar");
    testEqual(exc->className(), "Exception");
    testEqual(exc->message(), "bar");
    testEqual(exc->fullMessage(), "Exception: bar");
    exc->setPos(TokenPos("", 1, 0));
    testEqual(exc->fullMessage(), "Exception: bar at line 1");

    exc = gc.create<Exception>(TypeError::ObjectClass, "baz");
    testEqual(exc->fullMessage(), "TypeError: baz");

    Root<String*> str(gc.create<String>("foo"));
    exc = gc.create<Exception>(TypeError::ObjectClass, str);
    testEqual(exc->fullMessage(), "TypeError: foo");

    testExpectingException = false;
}
