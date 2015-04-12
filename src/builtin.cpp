#include "bool.h"
#include "builtin.h"
#include "callable.h"
#include "common.h"
#include "exception.h"
#include "input.h"
#include "integer.h"
#include "singletons.h"
#include "string.h"
#include "value-inl.h"

#include <iostream>

GlobalRoot<Object*> Builtin;

static bool builtin_hasattr(TracedVector<Value> args, Root<Value>& resultOut)
{
    Object* n = args[1].toObject();
    if (!n->is<String>()) {
        resultOut = gc::create<Exception>("TypeError",
                                  "hasattr(): attribute name must be string");
        return false;
    }
    Name name = n->as<String>()->value();
    resultOut = Boolean::get(args[0].toObject()->hasAttr(name));
    return true;
}

static bool builtin_object(TracedVector<Value> args, Root<Value>& resultOut)
{
    // todo: the returned object should not allow attributes to be set on it
    resultOut = gc::create<Object>();
    return true;
}

static bool builtin_isinstance(TracedVector<Value> args, Root<Value>& resultOut)
{
    // todo: support tuples etc for classInfo
    Root<Class*> cls(args[1].toObject()->as<Class>());
    resultOut = Boolean::get(args[0].toObject()->isInstanceOf(cls));
    return true;
}

void initBuiltins()
{
    Builtin.init(gc::create<Object>());
    Root<Value> value;
    initNativeMethod(Builtin, "hasattr", builtin_hasattr, 2);
    initNativeMethod(Builtin, "object", builtin_object, 0);
    initNativeMethod(Builtin, "isinstance", builtin_isinstance, 2);

    value = Boolean::True; Builtin->setAttr("True", value);
    value = Boolean::False; Builtin->setAttr("False", value);
    value = None; Builtin->setAttr("None", value);
    value = NotImplemented; Builtin->setAttr("NotImplemented", value);

    value = Exception::ObjectClass; Builtin->setAttr("Exception", value);

    string filename = "lib/builtin.py";
    string text = readFile(filename);
    Root<Block*> block;
    Block::buildModule(Input(text, filename), Builtin, block);
    Value result;
    if (!Interpreter::exec(block, result)) {
        printException(result);
        exit(1);
    }
}

#ifdef BUILD_TESTS

#include "test.h"

testcase(builtin)
{
    testInterp("hasattr(1, 'foo')", "False");
    testInterp("hasattr(1, '__str__')", "True");
    testInterp("a = object()\n"
               "a.foo = 1\n"
               "a.foo",
               "1");
    testInterp("range(4, 7)", "[4, 5, 6]");
}

#endif
