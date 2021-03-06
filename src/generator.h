#ifndef __GENERATOR_H__
#define __GENERATOR_H__

#include "object.h"
#include "callable.h"

struct ExceptionHandler;
struct Interpreter;

struct GeneratorIter : public Object
{
    static void init();
    static GlobalRoot<Class*> ObjectClass;

    GeneratorIter(Traced<Block*> block, Traced<Env*> env, unsigned argCount);

    virtual void traceChildren(Tracer& t) override;

    bool iter(MutableTraced<Value> resultOut);
    bool next(MutableTraced<Value> resultOut);

    void resume(Interpreter& interp);
    void leave(Interpreter& interp);
    void suspend(Interpreter& interp, Traced<Value> value);

  private:
    enum State
    {
        Running,
        Suspended,
        Finished
    };

    // todo: does it make sense to embed a Frame here?
    State state_;
    Heap<Block*> block_;
    Heap<Env*> env_;
    Heap<ExceptionHandler*> exceptionHandlers_;
    size_t ipOffset_;
    unsigned argCount_;
    HeapVector<Value> savedStack_;
};

#endif
