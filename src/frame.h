#ifndef __FRAME_H__
#define __FRAME_H__

#include "object.h"

struct Block;
struct ExceptionHandler;
struct InstrThunk;
struct Interpreter;

// Lexical environment object forming a linked list through parent pointer.
struct Env : public Object
{
    static GlobalRoot<Class*> ObjectClass;
    static GlobalRoot<Layout*> InitialLayout;
    static void init();

    Env(Traced<Env*> parent = nullptr,
        Traced<Layout*> layout = InitialLayout,
        Traced<Class*> cls = ObjectClass);

    Env* parent() const { return parent_; }

    void traceChildren(Tracer& t) override;

  private:
    Heap<Env*> parent_;
};

// Activation frame.
struct Frame
{
    Frame(); // so we can put these in a RootVector.
    Frame(InstrThunk* returnPoint, Traced<Block*> block, unsigned stackPos,
          unsigned extraPopCount);

    void setEnv(Env* env) {
        assert(!env_);
        env_ = env;
    }

    Block* block() const { return block_; }
    Env* env() const { return env_; }
    InstrThunk* returnPoint() const { return returnPoint_; }
    unsigned stackPos() const { return stackPos_; }
    unsigned extraPopCount() const { return extraPopCount_; }
    ExceptionHandler* exceptionHandlers() const { return exceptionHandlers_; }

    void pushHandler(Traced<ExceptionHandler*> eh);
    ExceptionHandler* popHandler();

    void setHandlers(Traced<ExceptionHandler*> ehs);
    ExceptionHandler* takeHandlers();

    void traceChildren(Tracer& t);

  private:
    Heap<Block*> block_;
    Heap<Env*> env_;
    InstrThunk* returnPoint_;
    unsigned stackPos_;
    unsigned extraPopCount_;
    Heap<ExceptionHandler*> exceptionHandlers_;
};

#endif
