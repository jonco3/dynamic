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
    Callable(Traced<Class*> cls, Name name, unsigned minArgs, unsigned maxArgs);
    Name name() { return name_; }
    unsigned minArgs() { return minArgs_; }
    unsigned maxArgs() { return maxArgs_; }

  private:
    Name name_;
    unsigned minArgs_;
    unsigned maxArgs_;
};

struct Native : public Callable
{
    static void init();
    static GlobalRoot<Class*> ObjectClass;

    typedef bool (*Func)(TracedVector<Value>, Root<Value>&);

    Native(Name name, Func func, unsigned minArgs, unsigned maxArgs = 0);

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

    Function(Name name,
             const vector<Name>& params,
             TracedVector<Value> defaults,
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
    Traced<Value> paramDefault(unsigned i) {
        assert(i < defaults_.size());
        return Traced<Value>::fromTracedLocation(&defaults_[i]);
    }

    virtual void traceChildren(Tracer& t) override {
        Callable::traceChildren(t);
        for (auto i: defaults_)
            gc::trace(t, &i);
        gc::trace(t, &block_);
        gc::trace(t, &scope_);
    }

  private:
    // todo: move common stuff to GC'ed FunctionInfo class also referred to by
    // instr
    vector<Name> params_;
    vector<Value> defaults_;
    Block* block_;
    Frame* scope_;
    bool isGenerator_;
};

void initNativeMethod(Traced<Object*> cls, Name name, Native::Func func,
                      unsigned minArgs, unsigned maxArgs = 0);

#endif
