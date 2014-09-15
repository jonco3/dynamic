#include "frame.h"

#include "interp.h"

Class Frame::Class;

Frame::Frame(Interpreter& interp, Frame* prev) :
  Object(&Class),
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
