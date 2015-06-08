#include "frame.h"

#include "block.h"
#include "interp.h"

GlobalRoot<Class*> Frame::ObjectClass;
GlobalRoot<Layout*> Frame::InitialLayout;

void Frame::init()
{
    ObjectClass.init(Class::createNative("Frame", nullptr));
    InitialLayout.init(Object::InitialLayout);
}

Frame::Frame(Traced<Block*> block) :
  Object(ObjectClass, Block::layout(block)),
  block_(block),
  returnPoint_(nullptr),
  stackPos_(0)
{}

void Frame::traceChildren(Tracer& t)
{
    Object::traceChildren(t);
    gc.trace(t, &block_);
}
