#include "builtin.h"
#include "callable.h"
#include "common.h"
#include "input.h"
#include "integer.h"
#include "singletons.h"
#include "value-inl.h"

#include <iostream>

static Value builtin_print(Value v)
{
    printf("%d\n", v.toObject()->as<Integer>()->value());
    return None;
}

#if 0
static Value builtin_hasattr(Value v, Value name)
{
    // todo: strings!
    Object* n = name.toObject();
    if (!n->is<String>())
        return false; // todo: exceptions!
    return v.toObject()->hasAttr(n->as<String>());
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
        cerr << "Error loading builtins" << endl;
        exit(1);
    }
}
