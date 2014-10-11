#include "frame.h"

#include "block.h"
#include "interp.h"

Class* Frame::ObjectClass;

void Frame::init()
{
    gc::addRoot(&ObjectClass);
    ObjectClass = new Class("Frame");
}

Frame::Frame(Frame* prev, Block* block) :
  Object(ObjectClass, block->layout()),
  prev(prev),
  next(nullptr),
  block_(block),
  retInstr(nullptr),
  pos(0)
{
    if (prev) {
        assert(!prev->next);
        prev->next = this;
    }
}

void Frame::setReturn(Interpreter& interp)
{
    assert(!retInstr);
    retInstr = interp.nextInstr();
    pos = interp.stackPos();
}

Frame* Frame::popFrame()
{
    assert(!next);
    if (prev)
        prev->next = nullptr;
    return prev;
}

Frame* Frame::ancestor(unsigned count)
{
    Frame *f = this;
    while (count--) {
        f = f->prev;
        assert(f);
    }
    return f;
}
