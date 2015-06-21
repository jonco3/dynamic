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

Frame::Frame()
  : block_(nullptr),
    env_(nullptr),
    returnPoint_(nullptr),
    stackPos_(0)
{}

Frame::Frame(Traced<Block*> block, Traced<Env*> env, unsigned stackPos)
  : block_(block),
    env_(env),
    returnPoint_(nullptr),
    stackPos_(stackPos)
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

void GCTraits<Frame>::trace(Tracer& t, Frame* frame)
{
    frame->traceChildren(t);
}

void GCTraits<Frame>::checkValid(const Frame& frame)
{
}
