#ifndef __FRAME_H__
#define __FRAME_H__

#include "object.h"
#include "class.h"

struct Block;
struct Instr;
struct Interpreter;

struct Frame : public Object
{
    static GlobalRoot<Class> ObjectClass;
    static void init();

    Frame(Frame* prev, Block* block);
    void setReturn(Interpreter& interp);

    Instr** returnInstr() { return retInstr; }
    unsigned stackPos() { return pos; }
    Frame* popFrame();
    const Block* block() const { return block_; }
    Frame* ancestor(unsigned count);

  private:
    Frame* prev;
    Frame* next;
    Block* block_;
    Instr** retInstr;
    unsigned pos;
};

#endif
