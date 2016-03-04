#include "common.h"

#include "builtin.h"
#include "callable.h"
#include "compiler.h"
#include "dict.h"
#include "exception.h"
#include "frame.h"
#include "file.h"
#include "input.h"
#include "interp.h"
#include "name.h"
#include "numeric.h"
#include "list.h"
#include "module.h"
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

void init1()
{
    Layout::init();
    initObject();
    initCallable();
    Env::init();
    Integer::init();
    Boolean::init();
    Float::init();
    Exception::init();
    initList();
    Slice::init();
    Dict::init();
    DictView::init();
    initSingletons();
    GeneratorIter::init();
    initReflect();
    Interpreter::init();
    initModules();
    File::Init();
}

void init2(const string& internalsPath)
{
    initBuiltins(internalsPath);
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

Env* createTopLevel()
{
    // todo: make the global object the end of the Env chain
    Stack<Env*> topLevel(gc.create<Env>());
    topLevel->setAttr(Names::__builtins__, Builtin);
    return topLevel;
}

bool execModule(string text, string filename, Traced<Env*> globals,
               MutableTraced<Value> resultOut)
{
    if (!CompileModule(Input(text, filename), globals, resultOut))
        return false;

    Stack<Block*> block(resultOut.as<CodeObject>()->block());
    return interp->exec(block, resultOut);
}

bool execModule(string text, string filename, Traced<Env*> globals)
{
    Stack<Value> result;
    bool ok = execModule(text, filename, globals, result);

    if (!ok)
        printException(result);
    return ok;
}
