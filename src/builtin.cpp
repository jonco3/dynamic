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

static bool builtin_hasattr(Traced<Value> v, Traced<Value> name, Root<Value>& resultOut)
{
    Object* n = name.toObject();
    if (!n->is<String>()) {
        resultOut = gc::create<Exception>("TypeError",
                                  "hasattr(): attribute name must be string");
        return false;
    }
    resultOut = Boolean::get(v.toObject()->hasAttr(n->as<String>()->value()));
    return true;
}

static bool builtin_object(Root<Value>& resultOut)
{
    // todo: the returned object should not allow attributes to be set on it
    resultOut = gc::create<Object>();
    return true;
}

void initBuiltins()
{
    Builtin.init(gc::create<Object>());
    Root<Value> value;
    value = gc::create<Native2>(builtin_hasattr); Builtin->setAttr("hasattr", value);
    value = gc::create<Native0>(builtin_object); Builtin->setAttr("object", value);

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
