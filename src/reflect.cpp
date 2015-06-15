#include "reflect.h"

#include "callable.h"
#include "instr.h"
#include "syntax.h"

GlobalRoot<Class*> ParseTree::ObjectClass;
GlobalRoot<Class*> CodeObject::ObjectClass;

ParseTree::ParseTree(unique_ptr<Syntax> syntax)
  : Object(ObjectClass), syntax_(move(syntax))
{}

void ParseTree::dump(ostream& s) const
{
    s << *syntax_.get() << endl;
}

CodeObject::CodeObject(Traced<Block*> block)
  : Object(ObjectClass), block_(block)
{}

void CodeObject::traceChildren(Tracer& t)
{
    gc.trace(t, &block_);
}

void CodeObject::dump(ostream& s) const
{
    s << *block_ << endl;
}

void initReflect()
{
    Stack<Class*> cls(Class::createNative("code object", nullptr));
    CodeObject::ObjectClass.init(cls);

    cls = Class::createNative("parse tree", nullptr);
    ParseTree::ObjectClass.init(cls);
}
