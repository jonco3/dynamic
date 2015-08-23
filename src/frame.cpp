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

void Env::traceChildren(Tracer& t)
{
    Object::traceChildren(t);
    gc.trace(t, &parent_);
}

Frame::Frame()
  : block_(nullptr),
    env_(nullptr),
    returnPoint_(nullptr),
    stackPos_(0),
    exceptionHandlers_(nullptr)
{}

Frame::Frame(InstrThunk* returnPoint, Traced<Block*> block, unsigned stackPos)
  : block_(block),
    env_(nullptr),
    returnPoint_(returnPoint),
    stackPos_(stackPos),
    exceptionHandlers_(nullptr)
{}

void Frame::pushHandler(Traced<ExceptionHandler*> eh)
{
    eh->setNext(exceptionHandlers_);
    exceptionHandlers_ = eh;
}

ExceptionHandler* Frame::popHandler()
{
    assert(exceptionHandlers_);
    ExceptionHandler* eh = exceptionHandlers_;
    exceptionHandlers_ = eh->next();
    return eh;
}

void Frame::setHandlers(Traced<ExceptionHandler*> ehs)
{
    assert(!exceptionHandlers_);
    exceptionHandlers_ = ehs;
}

ExceptionHandler* Frame::takeHandlers()
{
    ExceptionHandler* ehs = exceptionHandlers_;
    exceptionHandlers_ = nullptr;
    return ehs;
}

void Frame::traceChildren(Tracer& t)
{
    gc.trace(t, &block_);
    gc.trace(t, &env_);
    gc.trace(t, &exceptionHandlers_);
}

void GCTraits<Frame>::trace(Tracer& t, Frame* frame)
{
    frame->traceChildren(t);
}
