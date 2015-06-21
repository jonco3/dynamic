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
    static GlobalRoot<Class*> ObjectClass;

    Native(Name name, NativeFunc func, unsigned minArgs, unsigned maxArgs = 0);

    bool call(TracedVector<Value> args, MutableTraced<Value> resultOut) const {
        return func_(args, resultOut);
    }

  private:
    NativeFunc func_;
};

struct FunctionInfo : public Cell
{
    FunctionInfo(const vector<Name>& paramNames, Traced<Block*> block,
                 unsigned defaultCount = 0, bool takesRest = false,
                 bool isGenerator = false);

    virtual void traceChildren(Tracer& t) override {
        gc.trace(t, &block_);
    }

    size_t paramCount() const { return params_.size(); }

    vector<Name> params_;
    Block* block_;
    unsigned defaultCount_;
    bool takesRest_;
    bool isGenerator_;
};

struct Function : public Callable
{
    static GlobalRoot<Class*> ObjectClass;

    Function(Name name,
             Traced<FunctionInfo*> info,
             TracedVector<Value> defaults,
             Traced<Env*> env);

    Name paramName(unsigned i) {
        assert(i < info_->params_.size());
        return info_->params_[i];
    }

    Block* block() const { return info_->block_; }
    Env* env() const { return env_; }
    bool takesRest() const { return info_->takesRest_; }
    bool isGenerator() const { return info_->isGenerator_; }
    Traced<Value> paramDefault(unsigned i) {
        assert(i < info_->paramCount());
        i -= maxNormalArgs() - defaults_.size();
        assert(i < defaults_.size());
        return Traced<Value>::fromTracedLocation(&defaults_[i]);
    }
    size_t maxNormalArgs() const {
        return info_->paramCount() - (takesRest() ? 1 : 0);
    }
    unsigned restParam() const {
        assert(takesRest());
        return info_->paramCount() - 1;
    }

    void traceChildren(Tracer& t) override;

  private:
    FunctionInfo* info_;
    vector<Value> defaults_;
    Env* env_;
};

struct Method : public Object
{
    static GlobalRoot<Class*> ObjectClass;

    Method(Traced<Callable*> callable, Traced<Object*> object);

    void traceChildren(Tracer& t) override;

    Callable* callable() { return callable_; }
    Object* object() { return object_; }

  private:
    Callable* callable_;
    Object* object_;
};

extern void initCallable();

#endif
