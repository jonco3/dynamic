#ifndef __FRAME_H__
#define __FRAME_H__

#include "object.h"

struct Block;
struct Instr;
struct Interpreter;

struct Frame : public Object
{
    static GlobalRoot<Class*> ObjectClass;
    static GlobalRoot<Layout*> InitialLayout;
    static void init();

    Frame(Traced<Block*> block);
    void setStackPos(unsigned pos) { stackPos_ = pos; }
    void setReturnPoint(Instr** instrp) { returnPoint_ = instrp; }

    Instr** returnPoint() { return returnPoint_; }
    unsigned stackPos() { return stackPos_; }
    Block* block() { return block_; }

    virtual void traceChildren(Tracer& t) override;

  private:
    Block* block_;
    Instr** returnPoint_;
    unsigned stackPos_;
};

#endif
