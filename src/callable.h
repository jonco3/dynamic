#ifndef __CALLABLE_H__
#define __CALLABLE_H__

#include "object.h"
#include "interp.h"
#include "block.h"

#include <memory>
#include <vector>

using namespace std;

struct Callable : public Object
{
    Callable(Traced<Class*> cls, unsigned reqArgs);
    unsigned requiredArgs() { return reqArgs_; }

  private:
    unsigned reqArgs_;
};

struct Native : public Callable
{
    static void init();
    static GlobalRoot<Class*> ObjectClass;

    typedef bool (*Func)(TracedVector<Value>, Root<Value>&);

    Native(unsigned reqArgs, Func func);

    bool call(TracedVector<Value> args, Root<Value>& resultOut) const {
        return func_(args, resultOut);
    }

  private:
    Func func_;
};

struct Function : public Callable
{
    static void init();
    static GlobalRoot<Class*> ObjectClass;

    Function(const vector<Name>& params,
             Traced<Block*> block,
             Traced<Frame*> scope,
             bool isGenerator = false);

    Name paramName(unsigned i) {
        assert(i < params_.size());
        return params_[i];
    }

    Block* block() const { return block_; }
    Frame* scope() const { return scope_; }
    bool isGenerator() { return isGenerator_; }

    virtual void traceChildren(Tracer& t) override {
        Callable::traceChildren(t);
        gc::trace(t, &block_);
        gc::trace(t, &scope_);
    }

  private:
    // todo: move common stuff to GC'ed FunctionInfo class also referred to by
    // instr
    const vector<Name>& params_;
    Block* block_;
    Frame* scope_;
    bool isGenerator_;
};


#endif
