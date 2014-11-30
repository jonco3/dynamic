#ifndef __FRAME_H__
#define __FRAME_H__

#include "object.h"
#include "class.h"

struct Block;
struct Instr;
struct Interpreter;

struct Frame : public Object
{
    static GlobalRoot<Class*> ObjectClass;
    static void init();

    Frame(Traced<Block*> block, Instr** returnPoint = nullptr);
    void setStackPos(unsigned pos) { stackPos_ = pos; }

    Instr** returnPoint() { return returnPoint_; }
    unsigned stackPos() { return stackPos_; }
    Block* block() { return block_; }

    virtual void traceChildren(Tracer& t);

  private:
    Block* block_;
    Instr** returnPoint_;
    unsigned stackPos_;
};

#endif
