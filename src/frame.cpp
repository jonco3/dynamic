#include "frame.h"

#include "block.h"
#include "interp.h"

GlobalRoot<Class*> Frame::ObjectClass;

void Frame::init()
{
    ObjectClass.init(new Class("Frame"));
}

Frame::Frame(Block* block, Instr** returnPoint) :
  Object(ObjectClass, block->layout()),
  block_(block),
  returnPoint_(returnPoint),
  stackPos_(0)
{}
