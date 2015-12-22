#include "builtin.h"
#include "callable.h"
#include "common.h"
#include "compiler.h"
#include "dict.h"
#include "exception.h"
#include "input.h"
#include "interp.h"
#include "numeric.h"
#include "list.h"
#include "parser.h"
#include "reflect.h"
#include "singletons.h"
#include "string.h"
#include "value-inl.h"

#include <iostream>
#include <sstream>

GlobalRoot<Object*> Builtin;
GlobalRoot<Class*> SequenceIterator;
GlobalRoot<Function*> IterableToList;
bool builtinsInitialised = false;

static bool builtin_hasattr(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    Object* n = args[1].toObject();
    if (!n->is<String>()) {
        resultOut = gc.create<TypeError>(
            "hasattr(): attribute name must be string");
        return false;
    }
    Name name(n->as<String>()->value());
    resultOut = Boolean::get(args[0].toObject()->hasAttr(name));
    return true;
}

static bool builtin_isinstance(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    // todo: support tuples etc for classInfo
    Stack<Class*> cls(args[1].as<Class>());
    resultOut = Boolean::get(args[0].toObject()->isInstanceOf(cls));
    return true;
}

static bool builtin_parse(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], String::ObjectClass, resultOut))
        return false;

    string source = args[0].asObject()->as<String>()->value();
    SyntaxParser parser;
    parser.start(source);
    try {
        resultOut = gc.create<ParseTree>(parser.parseModule());
    } catch (const ParseError& e) {
        ostringstream s;
        s << e.what() << " at " << e.pos.file << " line " << dec << e.pos.line;
        resultOut = gc.create<SyntaxError>(s.str());
        return false;
    }

    return true;
}

static bool builtin_compile(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], String::ObjectClass, resultOut))
        return false;

    string source = args[0].asObject()->as<String>()->value();
    Stack<Block*> block;
    try {
        CompileModule(source, None, block);
    } catch (const ParseError& e) {
        SyntaxError* err = gc.create<SyntaxError>(e.what());
        err->setPos(e.pos);
        resultOut = err;
        return false;
    }

    resultOut = gc.create<CodeObject>(block);
    return true;
}

static Value make_builtin_iter()
{
    Stack<Layout*> layout(Env::InitialLayout);
    layout = layout->addName(Name::iterable);
    Stack<Block*> block(gc.create<Block>(layout, 1, false));
    block->append<Instr_InitStackLocals>();
    block->append<Instr_GetIterator>();
    block->append<Instr_Return>();
    block->setMaxStackDepth(1);
    Stack<Env*> env; // todo: allow construction of traced for nullptr
    vector<Name> args = { Name::iterable };
    Stack<FunctionInfo*> info(gc.create<FunctionInfo>(args, block));
    Name name("iter");
    return gc.create<Function>(name, info, EmptyValueArray, env);
}

static bool builtin_locals(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    // "Update and return a dictionary representing the current local symbol
    //  table.
    //  Note: The contents of this dictionary should not be modified; changes
    //  may not affect the values of local and free variables used by the
    //  interpreter."

    Stack<Dict*> dict(gc.create<Dict>());
    Frame* frame = interp->getFrame();
    Stack<Layout*> layout(frame->block()->layout());
    Stack<Env*> env(frame->env());
    while (layout != Layout::Empty) {
        unsigned index = layout->slotIndex();
        Stack<Value> name(String::get(layout->name()));
        Stack<Value> value(env ? env->getSlot(index) : interp->getStackLocal(index));
        dict->setitem(name, value);
        layout = layout->parent();
    }
    resultOut = Value(dict);
    return true;
}

void initBuiltins(const string& libDir)
{
    Builtin.init(Object::create());
    Stack<Value> value;

    // Functions
    initNativeMethod(Builtin, "hasattr", builtin_hasattr, 2);
    initNativeMethod(Builtin, "isinstance", builtin_isinstance, 2);
    initNativeMethod(Builtin, "compile", builtin_compile, 1);
    initNativeMethod(Builtin, "parse", builtin_parse, 1);
    value = make_builtin_iter(); initAttr(Builtin, "iter", value);
    initNativeMethod(Builtin, "locals", builtin_locals, 0);

    // Constants
    initAttr(Builtin, "True", Boolean::True);
    initAttr(Builtin, "False", Boolean::False);
    initAttr(Builtin, "None", None);
    initAttr(Builtin, "NotImplemented", NotImplemented);

    // Classes
    initAttr(Builtin, "object", Object::ObjectClass);
    initAttr(Builtin, "list", List::ObjectClass);
    initAttr(Builtin, "tuple", Tuple::ObjectClass);
    initAttr(Builtin, "dict", Dict::ObjectClass);
    initAttr(Builtin, "bool", Boolean::ObjectClass);
    initAttr(Builtin, "int", Integer::ObjectClass);
    initAttr(Builtin, "float", Float::ObjectClass);
    initAttr(Builtin, "str", String::ObjectClass);

    // Exceptions
    initAttr(Builtin, "Exception", Exception::ObjectClass);
#define set_exception_attr(cls)                                              \
    initAttr(Builtin, #cls, cls::ObjectClass);
for_each_exception_class(set_exception_attr)
#undef set_exception_attr

    string filename = libDir + "/builtin.py";
    string text = readFile(filename);
    if (!runModule(text, filename, Builtin))
        exit(1);

    Stack<Object*> internals(createTopLevel());
    filename = libDir + "/internal.py";
    text = readFile(filename);
    if (!runModule(text, filename, internals))
        exit(1);

    value = internals->getAttr(Name("SequenceIterator"));
    SequenceIterator.init(value.as<Class>());

    value = internals->getAttr(Name("iterableToList"));
    IterableToList.init(value.as<Function>());

    builtinsInitialised = true;
}

void finalBuiltins()
{
    builtinsInitialised = false;
}
