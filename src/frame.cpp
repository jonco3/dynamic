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

Env::Env(Traced<Env*> parent, Traced<Layout*> layout)
  : Object(ObjectClass, layout), parent_(parent)
{}

Frame::Frame()
  : block_(nullptr),
    env_(nullptr),
    returnPoint_(nullptr),
    stackPos_(0)
{}

Frame::Frame(InstrThunk* returnPoint, Traced<Block*> block, unsigned stackPos)
  : block_(block),
    env_(nullptr),
    returnPoint_(returnPoint),
    stackPos_(stackPos)
{}

void Frame::setEnv(Traced<Env*> env)
{
    assert(!env_);
    env_ = env;
}

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
