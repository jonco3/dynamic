#ifndef __FRAME_H__
#define __FRAME_H__

#include "object.h"
#include "class.h"

struct Instr;
struct Interpreter;

struct Frame : public Object
{
    static Class* ObjectClass;
    static void init();

    Frame(Interpreter& interp, Frame* prev, const Layout* layout);
    Instr** returnInstr() { return retInstr; }
    unsigned stackPos() { return pos; }
    Frame* popFrame();

  private:
    Frame* prev;
    Frame* next;
    Instr** retInstr;
    unsigned pos;
};

#endif
