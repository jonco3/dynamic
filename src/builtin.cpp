#include "builtin.h"
#include "callable.h"
#include "common.h"
#include "input.h"
#include "integer.h"
#include "singletons.h"
#include "value-inl.h"

#include <iostream>

static bool builtin_print(Value v, Value& resultOut)
{
    printf("%d\n", v.toObject()->as<Integer>()->value());
    resultOut = None;
    return true;
}

#if 0
static Value builtin_hasattr(Value v, Value name, Value& resultOut)
{
    // todo: strings!
    Object* n = name.toObject();
    if (!n->is<String>()) {
        resultOut = new Exception("TypeError: hasattr(): attribute name must be string");
        return false;
    }
    resultOut = v.toObject()->hasAttr(n->as<String>());
    return true;
}
#endif

void initBuiltins()
{
    Builtin.init(new Object);
    Root<Value> value;
    value = new Native1(builtin_print);
    Builtin->setAttr("print", value);

    string filename = "lib/builtin.py";
    string text = readFile(filename);
    Root<Block*> block(Block::buildModule(Input(text, filename), Builtin));
    Value result;
    Interpreter interp;
    if (!interp.interpret(block, result)) {
        printException(result);
        exit(1);
    }
}
