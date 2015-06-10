#include "common.h"

#include "bool.h"
#include "builtin.h"
#include "callable.h"
#include "dict.h"
#include "exception.h"
#include "frame.h"
#include "input.h"
#include "numeric.h"
#include "list.h"
#include "object.h"
#include "parser.h"
#include "reflect.h"
#include "singletons.h"
#include "string.h"
#include "list.h"
#include "generator.h"

#include <fstream>
#include <sstream>

bool debugMode = true;

void init1()
{
    Layout::init();
    initObject();
    Frame::init();
    Boolean::init();
    Integer::init();
    Float::init();
    String::init();
    Exception::init();
    initList();
    Dict::init();
    initSingletons();
    Interpreter::init();
    GeneratorIter::init();
    initReflect();
}

void init2(const string& libDir)
{
    initBuiltins(libDir);
}

void final()
{
    // todo
    gc.shutdown();
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

Object* createTopLevel()
{
    Root<Object*> topLevel(Object::create());
    topLevel->setAttr("__builtins__", Builtin);
    return topLevel;
}

bool runModule(string text, string filename, Traced<Object*> globals,
               Root<Value>* maybeResultOut)
{
    Root<Value> result;
    bool ok;
    try {
        Root<Block*> block;
        Block::buildModule(Input(text, filename), globals, block);
        ok = Interpreter::exec(block, result);
    } catch (const ParseError& e) {
        ostringstream s;
        s << e.what() << " at " << e.pos.file << " line " << dec << e.pos.line;
        result = gc.create<SyntaxError>(s.str());
        ok = false;
    }

    if (!ok)
        printException(result);
    if (maybeResultOut)
        *maybeResultOut = result;
    return ok;
}
