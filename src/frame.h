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

    Frame(Frame* prev, Block* block, Instr** returnPoint = nullptr);
    void setStackPos(unsigned pos) { stackPos_ = pos; }

    Instr** returnPoint() { return returnPoint_; }
    unsigned stackPos() { return stackPos_; }
    Frame* popFrame();
    Block* block() { return block_; }
    Frame* ancestor(unsigned count);

  private:
    Frame* prev_;
    Frame* next_;
    Block* block_;
    Instr** returnPoint_;
    unsigned stackPos_;
};

#endif
