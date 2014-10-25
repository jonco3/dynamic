#include "frame.h"

#include "block.h"
#include "interp.h"

GlobalRoot<Class*> Frame::ObjectClass;

void Frame::init()
{
    ObjectClass.init(new Class("Frame"));
}

Frame::Frame(Frame* prev, Block* block, Instr** returnPoint) :
  Object(ObjectClass, block->layout()),
  prev_(prev),
  next_(nullptr),
  block_(block),
  returnPoint_(returnPoint),
  stackPos_(0)
{
    if (prev_) {
        assert(!prev_->next_);
        prev_->next_ = this;
    }
}

Frame* Frame::popFrame()
{
    assert(!next_);
    if (prev_) {
        assert(prev_->next_ == this);
        prev_->next_ = nullptr;
    }
    return prev_;
}

Frame* Frame::ancestor(unsigned count)
{
    Frame *f = this;
    while (count--) {
        f = f->prev_;
        assert(f);
    }
    return f;
}
