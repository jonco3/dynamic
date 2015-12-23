#include "common.h"

#include "builtin.h"
#include "callable.h"
#include "compiler.h"
#include "dict.h"
#include "exception.h"
#include "frame.h"
#include "input.h"
#include "interp.h"
#include "name.h"
#include "numeric.h"
#include "list.h"
#include "object.h"
#include "parser.h"
#include "reflect.h"
#include "singletons.h"
#include "slice.h"
#include "string.h"
#include "list.h"
#include "generator.h"

#include <fstream>
#include <sstream>

bool debugMode = true;

void crash(const char* message)
{
    fprintf(stderr, "Abort: %s", message);
    exit(1);
}

void init1()
{
    Layout::init();
    initObject();
    //initNames();
    initCallable();
    Env::init();
    Integer::init();
    Boolean::init();
    Float::init();
    String::init();
    Exception::init();
    initList();
    Slice::init();
    Dict::init();
    initSingletons();
    GeneratorIter::init();
    initReflect();
    Interpreter::init();
}

void init2(const string& libDir)
{
    initBuiltins(libDir);
}

void final()
{
    // todo
    finalBuiltins();
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
    Stack<Object*> topLevel(Object::create());
    topLevel->setAttr(Names::__builtins__, Builtin);
    return topLevel;
}

bool runModule(string text, string filename, Traced<Object*> globals,
               Stack<Value>* maybeResultOut)
{
    Stack<Value> result;
    bool ok;
    try {
        Stack<Block*> block;
        CompileModule(Input(text, filename), globals, block);
        ok = interp->exec(block, result);
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
