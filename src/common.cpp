#include "common.h"

#include "bool.h"
#include "callable.h"
#include "class.h"
#include "frame.h"
#include "input.h"
#include "integer.h"
#include "object.h"
#include "singletons.h"
#include "test.h"

#include <fstream>

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

string readFile(string filename)
{
    ifstream file(filename);
    stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool runModule(string text, string filename)
{
    Root<Block*> block(Block::buildModule(Input(text, filename)));
    Value result;
    Interpreter interp;
    return interp.interpret(block, result);
}

bool runStatements(string text, string filename)
{
    Root<Block*> block(Block::buildStatements(Input(text, filename)));
    Value result;
    Interpreter interp;
    bool ok = interp.interpret(block, result);
    if (!ok)
        cout << "Error" << endl;
    else
        cout << repr(result) << endl;
    return ok;
}
