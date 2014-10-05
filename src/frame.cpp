#include "frame.h"

#include "block.h"
#include "interp.h"

Class* Frame::ObjectClass;

void Frame::init()
{
    ObjectClass = new Class("Frame");
}

Frame::Frame(Interpreter& interp, Frame* prev, Block* block) :
  Object(ObjectClass, block->getLayout()),
  prev(prev),
  next(nullptr),
  block_(block),
  retInstr(interp.nextInstr()),
  pos(interp.stackPos())
{
    if (prev) {
        assert(!prev->next);
        prev->next = this;
    }
}

Frame* Frame::popFrame()
{
    assert(!next);
    if (prev)
        prev->next = nullptr;
    return prev;
}
