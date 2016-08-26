#include "builtin.h"
#include "callable.h"
#include "common.h"
#include "compiler.h"
#include "dict.h"
#include "exception.h"
#include "file.h"
#include "input.h"
#include "interp.h"
#include "numeric.h"
#include "list.h"
#include "module.h"
#include "parser.h"
#include "reflect.h"
#include "singletons.h"
#include "set.h"
#include "string.h"
#include "value-inl.h"

#include <iostream>
#include <sstream>

GlobalRoot<Env*> Builtin;
GlobalRoot<Class*> SequenceIterator;
GlobalRoot<Function*> IterableToList;
GlobalRoot<Function*> InUsingIteration;
GlobalRoot<Function*> InUsingSubscript;
GlobalRoot<Function*> LoadModule;
bool builtinsInitialised = false;

static bool builtin_hasattr(NativeArgs args,
                            MutableTraced<Value> resultOut)
{
    Object* n = args[1].toObject();
    if (!n->is<String>()) {
        return Raise<TypeError>("hasattr(): attribute name must be string",
                                resultOut);
    }
    Name name(internString(n->as<String>()->value()));
    resultOut = Boolean::get(args[0].toObject()->hasAttr(name));
    return true;
}

static bool builtin_isinstance(NativeArgs args,
                               MutableTraced<Value> resultOut)
{
    // todo: support tuples etc for classInfo
    Stack<Class*> cls(args[1].as<Class>());
    resultOut = Boolean::get(args[0].toObject()->isInstanceOf(cls));
    return true;
}

static bool builtin_parse(NativeArgs args,
                          MutableTraced<Value> resultOut)
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
        return Raise<SyntaxError>(s.str(), resultOut);
    }

    return true;
}

static bool builtin_compile(NativeArgs args,
                            MutableTraced<Value> resultOut)
{
    if (!checkInstanceOf(args[0], String::ObjectClass, resultOut))
        return false;

    string source = args[0].asObject()->as<String>()->value();
    Stack<Env*> globals;
    return CompileModule(source, globals, resultOut);
}

static Value make_builtin_iter()
{
    Stack<Env*> global;
    Stack<Layout*> layout(Env::InitialLayout);
    layout = layout->addName(Names::iterable);
    Stack<Block*> parent;
    Stack<Block*> block(gc.create<Block>(parent, global, layout, 1, false));
    block->append<Instr_InitStackLocals>(0);
    block->append<Instr_GetIterator>();
    block->append<Instr_Return>();
    block->setMaxStackDepth(1);
    vector<Name> args = { Names::iterable };
    Stack<FunctionInfo*> info(gc.create<FunctionInfo>(args, block));
    return gc.create<Function>(Names::iter, info, EmptyValueArray, nullptr);
}

static Env* GetOrCreateLocals()
{
    Frame* frame = interp->getFrame();
    Stack<Env*> env(frame->env());
    if (env && frame->block()->createEnv())
        return env;

    // todo: this is a check to see if we're in global scope
    if (!frame->block()->parent())
        return frame->block()->global();

    Stack<Layout*> layout(frame->block()->layout());
    Stack<Env*> parent;
    env = gc.create<Env>(parent, layout);
    while (layout != Layout::Empty) {
        unsigned index = layout->slotIndex();
        Stack<Value> name(layout->name());
        Stack<Value> value(interp->getStackLocal(index));
        env->setSlot(index, value);
        layout = layout->parent();
    }

    return env;
}

static bool builtin_locals(NativeArgs args,
                           MutableTraced<Value> resultOut)
{
    // "Update and return a dictionary representing the current local symbol
    //  table.
    //  Note: The contents of this dictionary should not be modified; changes
    //  may not affect the values of local and free variables used by the
    //  interpreter."

    Stack<Env*> env(GetOrCreateLocals());
    resultOut = gc.create<DictView>(env);
    return true;
}

static bool builtin_globals(NativeArgs args,
                            MutableTraced<Value> resultOut)
{
    Stack<Object*> global(interp->getFrame()->block()->global());
    Stack<DictView*> dict(gc.create<DictView>(global));
    resultOut = Value(dict);
    return true;
}

static Layout* CreateLayoutForDict(Traced<Dict*> dict)
{
    Stack<Layout*> layout(Layout::Empty);
    for (const auto& i : dict->entries()) {
        Stack<Value> key(i.first);
        if (key.is<String>()) {
            Name name = internString(key.as<String>()->value());
            layout = layout->addName(name);
        }
    }
    return layout;
}

static void SetAttrsFromDict(Traced<Dict*> dict, Traced<Object*> object)
{
    unsigned slot = 0;
    for (const auto& i : dict->entries()) {
        Stack<Value> key(i.first);
        if (key.is<String>())
            object->setSlot(slot++, i.second);
    }
}

// todo: we create an Env object even for globals, which is currently not
// required but it doesn't matter.
static Env* DictToEnv(Traced<Value> arg, MutableTraced<Value> resultOut)
{
    if (arg.is<DictView>()) {
        // todo: currently correct, should drive Env through DictView
        return arg.as<DictView>()->target()->as<Env>();
    }

    if (!arg.is<Dict>()) {
        Raise<TypeError>("eval() arg 2 must be a dict", resultOut);
        return nullptr;
    }

    Stack<Dict*> dict(arg.as<Dict>());
    Stack<Layout*> layout(CreateLayoutForDict(dict));
    Stack<Env*> parent;
    Stack<Env*> result(gc.create<Env>(parent, layout));
    SetAttrsFromDict(dict, result);
    return result;
}

static void UpdateDictFromEnv(Traced<Value> arg, Traced<Env*> env)
{
    if (!arg.is<Dict>())
        return;

    Stack<Dict*> dict(arg.as<Dict>());
    dict->clear();

    Stack<Layout*> layout;
    for (layout = env->layout();
         layout != Layout::Empty;
         layout = layout->parent())
    {
        Stack<Value> key(layout->name());
        Stack<Value> value(env->getSlot(layout->slotIndex()));
        dict->setitem(key, value);
    }
}

static bool builtin_eval(NativeArgs args,
                         MutableTraced<Value> resultOut)
{
    Stack<Value> expression(args[0]);
    if (!expression.is<String>()) {
        // todo: support code objects
        return Raise<TypeError>("eval() arg 1 must be a string", resultOut);
    }

    Stack<Env*> globals;
    if (args.size() >= 2 && args[1].toObject() != None) {
        globals = DictToEnv(args[1], resultOut);
        if (!globals)
            return false;
    } else {
        globals = interp->getFrame()->block()->global();
    }

    Stack<Env*> locals;
    if (args.size() == 3) {
        locals = DictToEnv(args[2], resultOut);
        if (!locals)
            return false;
    } else {
        locals = GetOrCreateLocals();
    }

    if (!globals->hasAttr(Names::__builtins__)) {
        Stack<Object*> current(interp->getFrame()->block()->global());
        Stack<Value> builtins(current->getAttr(Names::__builtins__));
        globals->setAttr(Names::__builtins__, builtins);
    }

    const string& text = expression.as<String>()->value();
    if (!CompileEval(Input(text, "<eval>"), globals, locals, resultOut))
        return false;

    Stack<Value> function(resultOut);
    if (!interp->call(function, resultOut))
        return false;

    if (args.size() >= 2 && args[1].toObject() != None)
        UpdateDictFromEnv(args[1], globals);

    if (args.size() == 3)
        UpdateDictFromEnv(args[2], locals);

    return true;
}

static bool builtin_exec(NativeArgs args,
                         MutableTraced<Value> resultOut)
{
    // todo:
    // "Remember that at module level, globals and locals are the same
    //  dictionary. If exec gets two separate objects as globals and locals, the
    //  code will be executed as if it were embedded in a class definition."

    Stack<Value> expression(args[0]);
    if (!expression.is<String>()) {
        // todo: support code objects
        return Raise<TypeError>("exec() arg 1 must be a string", resultOut);
    }

    Stack<Env*> globals;
    if (args.size() >= 2 && args[1].toObject() != None) {
        globals = DictToEnv(args[1], resultOut);
        if (!globals)
            return false;
    } else {
        globals = interp->getFrame()->block()->global();
    }

    Stack<Env*> locals;
    if (args.size() == 3) {
        locals = DictToEnv(args[2], resultOut);
        if (!locals)
            return false;
    } else {
        locals = GetOrCreateLocals();
    }

    if (!globals->hasAttr(Names::__builtins__)) {
        Stack<Object*> current(interp->getFrame()->block()->global());
        Stack<Value> builtins(current->getAttr(Names::__builtins__));
        globals->setAttr(Names::__builtins__, builtins);
    }

    const string& text = expression.as<String>()->value();
    if (!CompileExec(Input(text, "<exec>"), globals, locals, resultOut))
        return false;

    Stack<Value> function(resultOut);
    if (!interp->call(function, resultOut))
        return false;

    if (args.size() >= 2 && args[1].toObject() != None)
        UpdateDictFromEnv(args[1], globals);

    if (args.size() == 3)
        UpdateDictFromEnv(args[2], locals);

    return true;
}

static bool internal_execModule(NativeArgs args,
                                MutableTraced<Value> resultOut)
{
    // Like exec, but creates a module object and returns it.

    assert(args.size() == 4);
    if (!args[0].is<String>() || !args[1].is<String>() ||
        !args[2].is<String>() || !args[3].is<String>())
    {
        return Raise<TypeError>("execModule() args must be strings", resultOut);
    }
    Stack<String*> name(args[0].as<String>());
    Stack<String*> package(args[1].as<String>());
    Stack<String*> source(args[2].as<String>());
    Stack<String*> filename(args[3].as<String>());

    Stack<Module*> module(gc.create<Module>(name, package));

    // todo: I've no idea if this is correct.
    Stack<Object*> current(interp->getFrame()->block()->global());
    Stack<Value> builtins(current->getAttr(Names::__builtins__));
    module->setAttr(Names::__builtins__, builtins);

    if (!execModule(source->value(), filename->value(), module, resultOut))
        return false;

    resultOut = Value(module);
    return true;
}

void initBuiltins(const string& internalsPath)
{
    Stack<String*> name(Names::builtins);
    Builtin.init(gc.create<Module>(name));
    Stack<Value> value;

    // Functions
    initNativeMethod(Builtin, "hasattr", builtin_hasattr, 2);
    initNativeMethod(Builtin, "isinstance", builtin_isinstance, 2);
    initNativeMethod(Builtin, "compile", builtin_compile, 1);
    initNativeMethod(Builtin, "parse", builtin_parse, 1);
    value = make_builtin_iter(); initAttr(Builtin, "iter", value);
    initNativeMethod(Builtin, "locals", builtin_locals, 0);
    initNativeMethod(Builtin, "globals", builtin_globals, 0);
    initNativeMethod(Builtin, "eval", builtin_eval, 1, 3);
    initNativeMethod(Builtin, "exec", builtin_exec, 1, 3);
    initNativeMethod(Builtin, "open", File::Open, 1, 2);

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
    initAttr(Builtin, "set", Set::ObjectClass);
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

    string filename = internalsPath + "/builtin.py";
    string text = readFile(filename);
    if (!execModule(text, filename, Builtin))
        exit(1);

    Stack<Env*> internals(createTopLevel());
    internals->setAttr(Names::sys, Module::Sys);
    initNativeMethod(internals, "execModule", internal_execModule, 4);

    filename = internalsPath + "/internal.py";
    text = readFile(filename);
    if (!execModule(text, filename, internals))
        exit(1);

    value = internals->getAttr(Names::SequenceIterator);
    SequenceIterator.init(value.as<Class>());

    value = internals->getAttr(Names::iterableToList);
    IterableToList.init(value.as<Function>());

    value = internals->getAttr(Names::inUsingIteration);
    InUsingIteration.init(value.as<Function>());

    value = internals->getAttr(Names::inUsingSubscript);
    InUsingSubscript.init(value.as<Function>());

    value = internals->getAttr(Names::loadModule);
    LoadModule.init(value.as<Function>());

    value = internals->getAttr(Names::__import__);
    Builtin->setAttr(Names::__import__, value);

    builtinsInitialised = true;
}

void finalBuiltins()
{
    builtinsInitialised = false;
}
