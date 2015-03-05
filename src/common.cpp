#include "common.h"

#include "bool.h"
#include "builtin.h"
#include "callable.h"
#include "class.h"
#include "dict.h"
#include "exception.h"
#include "frame.h"
#include "input.h"
#include "integer.h"
#include "list.h"
#include "object.h"
#include "parser.h"
#include "singletons.h"
#include "string.h"
#include "test.h"
#include "list.h"

#include <fstream>
#include <sstream>

bool debugMode = true;

void init1()
{
    Object::init();
    Class::init();
    Native::init();
    Function::init();
    Frame::init();
    Exception::init();
    Boolean::init();
    Integer::init();
    String::init();
    initList();
    Dict::init();
    initSingletons();
    Interpreter::init();
}

void init2()
{
    initBuiltins();
}

void final()
{
    // todo
}

string readFile(string filename)
{
    ifstream s(filename);
    if (!s) {
        cerr << "Can't open file: " << filename << endl;
        exit(1);
    }
    stringstream buffer;
    buffer << s.rdbuf();
    if (!s) {
        cerr << "Can't read file: " << filename << endl;
        exit(1);
    }
    return buffer.str();
}

void printException(Value value)
{
    Exception* ex = value.asObject()->as<Exception>();
    cerr << ex->fullMessage() << endl;
}

bool runModule(string text, string filename, Traced<Object*> globals, Value* valueOut)
{
    Value result;
    bool ok;
    try {
        Root<Block*> block;
        Block::buildModule(Input(text, filename), globals, block);
        ok = Interpreter::exec(block, result);
    } catch (const ParseError& e) {
        ostringstream s;
        s << e.what() << " at " << e.pos.file << " line " << dec << e.pos.line;
        result = gc::create<Exception>("SyntaxError", s.str());
        ok = false;
    }

    if (!ok)
        printException(result);
    if (valueOut)
        *valueOut = result;
    return ok;
}
