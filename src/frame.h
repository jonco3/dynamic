#ifndef __FRAME_H__
#define __FRAME_H__

#include "object.h"

struct Block;
struct InstrThunk;
struct Interpreter;

// Lexical environment object forming a linked list thought parent pointer.
struct Env : public Object
{
    static GlobalRoot<Class*> ObjectClass;
    static GlobalRoot<Layout*> InitialLayout;
    static void init();

    Env(Traced<Env*> parent, Traced<Block*> block);

    Env* parent() const { return parent_; }

    virtual void traceChildren(Tracer& t) override;

  private:
    Env* parent_;
};

// Activation frame.
struct Frame : public Cell
{
    Frame(Traced<Block*> block, Traced<Env*> env);
    void setStackPos(unsigned pos) { stackPos_ = pos; }
    void setReturnPoint(InstrThunk* instrp) { returnPoint_ = instrp; }

    Block* block() const { return block_; }
    Env* env() const { return env_; }
    InstrThunk* returnPoint() const { return returnPoint_; }
    unsigned stackPos() const { return stackPos_; }

    void traceChildren(Tracer& t);

  private:
    Block* block_;
    Env* env_;
    InstrThunk* returnPoint_;
    unsigned stackPos_;
};

#endif
