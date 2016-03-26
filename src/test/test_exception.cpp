#include "../exception.h"

#include "../test.h"
#include "../value-inl.h"

testcase(exception)
{
    Stack<Exception*> exc;
    testExpectingException = true;
    exc = gc.create<Exception>(Exception::ObjectClass, "bar");
    testEqual(exc->className(), "Exception");
    testEqual(exc->message(), "bar");
    testEqual(exc->fullMessage(), "Exception: bar");

    exc = gc.create<Exception>(TypeError::ObjectClass, "baz");
    testEqual(exc->fullMessage(), "TypeError: baz");

    Stack<String*> str(gc.create<String>("foo"));
    exc = gc.create<Exception>(TypeError::ObjectClass, str);
    testEqual(exc->fullMessage(), "TypeError: foo");

    testExpectingException = false;
}
