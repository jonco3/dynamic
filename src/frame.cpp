#include "frame.h"

#include "interp.h"

Class* Frame::ObjectClass;

void Frame::init()
{
    ObjectClass = new Class("Frame");
}

Frame::Frame(Interpreter& interp, Frame* prev, const Layout* layout) :
  Object(ObjectClass, layout),
  prev(prev),
  next(nullptr),
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
