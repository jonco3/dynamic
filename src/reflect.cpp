#include "reflect.h"

#include "callable.h"
#include "instr.h"
#include "syntax.h"

GlobalRoot<Class*> ParseTree::ObjectClass;
GlobalRoot<Class*> CodeObject::ObjectClass;

ParseTree::ParseTree(unique_ptr<Syntax> syntax)
  : Object(ObjectClass), syntax_(move(syntax))
{}

void ParseTree::dump() const
{
    cout << *syntax_.get() << endl;
}

CodeObject::CodeObject(Traced<Block*> block)
  : Object(ObjectClass), block_(block)
{}

void CodeObject::traceChildren(Tracer& t)
{
    gc.trace(t, &block_);
}

void CodeObject::dump() const
{
    cout << *block_ << endl;
}

static bool parseTree_dump(TracedVector<Value> args, Root<Value>& resultOut)
{
    if (!checkInstanceOf(args[0], ParseTree::ObjectClass, resultOut))
        return false;
    args[0].asObject()->as<ParseTree>()->dump();
    resultOut = None;
    return true;
}

static bool codeObject_dump(TracedVector<Value> args, Root<Value>& resultOut)
{
    if (!checkInstanceOf(args[0], CodeObject::ObjectClass, resultOut))
        return false;
    args[0].asObject()->as<CodeObject>()->dump();
    resultOut = None;
    return true;
}

void initReflect()
{
    Root<Class*> cls(gc.create<Class>("code object"));
    initNativeMethod(cls, "__dump__", codeObject_dump, 1);
    CodeObject::ObjectClass.init(cls);

    cls = gc.create<Class>("parse tree");
    initNativeMethod(cls, "__dump__", parseTree_dump, 1);
    ParseTree::ObjectClass.init(cls);
}
