#ifndef __FRAME_H__
#define __FRAME_H__

#include "object.h"

struct Block;
struct InstrThunk;
struct Interpreter;

struct Frame : public Object
{
    static GlobalRoot<Class*> ObjectClass;
    static GlobalRoot<Layout*> InitialLayout;
    static void init();

    Frame(Traced<Block*> block);
    void setStackPos(unsigned pos) { stackPos_ = pos; }
    void setReturnPoint(InstrThunk* instrp) { returnPoint_ = instrp; }

    InstrThunk* returnPoint() { return returnPoint_; }
    unsigned stackPos() { return stackPos_; }
    Block* block() { return block_; }

    virtual void traceChildren(Tracer& t) override;

  private:
    Block* block_;
    InstrThunk* returnPoint_;
    unsigned stackPos_;
};

#endif
