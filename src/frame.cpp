#include "frame.h"

#include "block.h"
#include "interp.h"

GlobalRoot<Class*> Env::ObjectClass;
GlobalRoot<Layout*> Env::InitialLayout;

void Env::init()
{
    ObjectClass.init(Class::createNative("Env", nullptr));
    InitialLayout.init(Object::InitialLayout);
}

Env::Env(Traced<Env*> parent, Traced<Block*> block)
  : Object(ObjectClass, Block::layout(block)), parent_(parent)
{}

Frame::Frame(Traced<Block*> block, Traced<Env*> env) :
  block_(block),
  env_(env),
  returnPoint_(nullptr),
  stackPos_(0)
{}

void Env::traceChildren(Tracer& t)
{
    Object::traceChildren(t);
    gc.trace(t, &parent_);
}

void Frame::traceChildren(Tracer& t)
{
    gc.trace(t, &block_);
    gc.trace(t, &env_);
}
