#ifndef __GENERATOR_H__
#define __GENERATOR_H__

#include "object.h"
#include "callable.h"

struct GeneratorIter : public Object
{
    static void init();
    static GlobalRoot<Class*> ObjectClass;

    GeneratorIter(Traced<Block*> block, Traced<Env*> env, unsigned argCount);

    virtual void traceChildren(Tracer& t) override;

    bool iter(MutableTraced<Value> resultOut);
    bool next(MutableTraced<Value> resultOut);

    bool resume(Interpreter& interp);
    bool leave(Interpreter& interp);
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
    Block* block_;
    Env* env_;
    size_t ipOffset_;
    unsigned argCount_;
    vector<Value> savedStack_;
};

#endif
