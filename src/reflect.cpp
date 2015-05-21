#include "reflect.h"

#include "callable.h"
#include "instr.h"

GlobalRoot<Class*> CodeObject::ObjectClass;

CodeObject::CodeObject(Traced<Block*> block)
  : Object(ObjectClass), block_(block)
{}

void CodeObject::traceChildren(Tracer& t)
{
    gc.trace(t, &block_);
}

static bool codeObject_dump(TracedVector<Value> args, Root<Value>& resultOut)
{
    if (!checkInstanceOf(args[0], CodeObject::ObjectClass, resultOut))
        return false;
    args[0].asObject()->as<CodeObject>()->dump();
    resultOut = None;
    return true;
}

void CodeObject::dump() const
{
    cout << *block_ << endl;
}

void initReflect()
{
    Root<Class*> cls(gc.create<Class>("code object"));
    initNativeMethod(cls, "__dump__", codeObject_dump, 1);
    CodeObject::ObjectClass.init(cls);
}
