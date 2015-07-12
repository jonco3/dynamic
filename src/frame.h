#ifndef __FRAME_H__
#define __FRAME_H__

#include "object.h"

struct Block;
struct InstrThunk;
struct Interpreter;

// Lexical environment object forming a linked list through parent pointer.
struct Env : public Object
{
    static GlobalRoot<Class*> ObjectClass;
    static GlobalRoot<Layout*> InitialLayout;
    static void init();

    Env(Traced<Env*> parent, Traced<Layout*> layout);

    Env* parent() const { return parent_; }

    virtual void traceChildren(Tracer& t) override;

  private:
    Env* parent_;
};

// Activation frame.
struct Frame
{
    Frame(); // so we can put these in a RootVector.
    Frame(InstrThunk* returnPoint, Traced<Block*> block,
          unsigned stackPos, unsigned argCount);

    void setEnv(Traced<Env*> env);

    Block* block() const { return block_; }
    Env* env() const { return env_; }
    InstrThunk* returnPoint() const { return returnPoint_; }
    unsigned stackPos() const { return stackPos_; }
    unsigned argCount() const { return argCount_; }

    void traceChildren(Tracer& t);

  private:
    Block* block_;
    Env* env_;
    InstrThunk* returnPoint_;
    unsigned stackPos_;
    unsigned argCount_;
};

#endif
