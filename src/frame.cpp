#include "frame.h"

#include "block.h"
#include "interp.h"

GlobalRoot<Class*> Frame::ObjectClass;

void Frame::init()
{
    ObjectClass.init(new Class("Frame"));
}

Frame::Frame(Traced<Block*> block, Instr** returnPoint) :
  Object(ObjectClass, Block::layout(block)),
  block_(block),
  returnPoint_(returnPoint),
  stackPos_(0)
{}

void Frame::traceChildren(Tracer& t)
{
    Object::traceChildren(t);
    gc::trace(t, &block_);
}
