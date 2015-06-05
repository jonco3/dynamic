#include "src/list.h"

#include "src/interp.h"
#include "src/test.h"

testcase(tuple)
{
    testInterp("()", "()");
    testInterp("(1,2)", "(1, 2)");
    testInterp("(1,2,)", "(1, 2)");
    testInterp("(1,)", "(1,)");
    testInterp("(1 + 2, 3 + 4)", "(3, 7)");
    testInterp("(1,)[0]", "1");
    testInterp("(1,2)[1]", "2");
    testInterp("(1,2)[-1]", "2");
    testInterp("2 in (1,)", "False");
    testInterp("2 in (1, 2, 3)", "True");

    testException("1[0]", "'int' object has no attribute '__getitem__'");
    testException("()[0]", "tuple index out of range");
    testException("(1,)[1]", "tuple index out of range");
    // todo: should be TypeError: 'tuple' object does not support item assignment
    testException("(1,)[0] = 2", "'tuple' object has no attribute '__setitem__'");
    // todo: should be TypeError: argument of type 'int' is not iterable
    testException("(1,) in 1", "TypeError: Argument is not iterable");
}

testcase(list)
{
    testInterp("[]", "[]");
    testInterp("[1,2]", "[1, 2]");
    testInterp("[3,4,]", "[3, 4]");
    testInterp("[1,]", "[1]");
    testInterp("[1 + 2, 3 + 4]", "[3, 7]");
    testInterp("[1][0]", "1");
    testInterp("[1,2][1]", "2");
    testInterp("[1,2][-1]", "2");
    testInterp("[1][0] = 2", "2");
    testInterp("a = [1]; a[0] = 3; a", "[3]");
    testInterp("2 in [1]", "False");
    testInterp("2 in [1, 2, 3]", "True");
    testInterp("[1].append(2)", "None");
    testInterp("a = [1]; a.append(2); a", "[1, 2]");

    testException("1[0]", "'int' object has no attribute '__getitem__'");
    testException("[][0]", "list index out of range");
    testException("[1][1]", "list index out of range");
}
