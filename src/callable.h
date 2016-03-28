#ifndef __CALLABLE_H__
#define __CALLABLE_H__

#include "object.h"

#include <memory>
#include <vector>

using namespace std;

struct Block;
struct Env;

struct Callable : public Object
{
    Callable(Traced<Class*> cls, Name name, unsigned minArgs, unsigned maxArgs);
    Name name() { return name_; }
    unsigned minArgs() { return minArgs_; }
    unsigned maxArgs() { return maxArgs_; }

    void print(ostream& s) const override;

  private:
    Name name_;
    unsigned minArgs_;
    unsigned maxArgs_;
};

struct Native : public Callable
{
    static GlobalRoot<Class*> ObjectClass;

    Native(Name name, NativeFunc func, unsigned minArgs, unsigned maxArgs = 0);

    bool call(NativeArgs args, MutableTraced<Value> resultOut) const {
        return func_(args, resultOut);
    }

  private:
    NativeFunc func_;
};

struct FunctionInfo : public Cell
{
    FunctionInfo(const vector<Name>& paramNames,
                 Traced<Block*> block,
                 unsigned defaultCount = 0,
                 int restParam = -1,
                 int keywordsParam = -1,
                 bool isGenerator = false);

    void traceChildren(Tracer& t) override;

    size_t argCount() const { return params_.size(); }
    bool takesRest() const { return restParam_ != -1; }
    bool takesKeywords() const { return keywordsParam_ != -1; }
    unsigned minArgs() const;
    unsigned maxArgs() const;

    vector<Name> params_;
    Heap<Block*> block_;
    unsigned defaultCount_;
    int restParam_;
    int keywordsParam_;
    bool isGenerator_;
};

struct Function : public Callable
{
    static GlobalRoot<Class*> ObjectClass;

    Function(Name name,
             Traced<FunctionInfo*> info,
             TracedVector<Value> defaults,
             Traced<Env*> env = nullptr);

    Name paramName(unsigned i) {
        assert(i < info_->params_.size());
        return info_->params_[i];
    }

    Block* block() const { return info_->block_; }
    Env* env() const { return env_; }
    bool takesRest() const  { return info_->takesRest(); }
    bool takesKeywords() const  { return info_->takesKeywords(); }
    bool isGenerator() const { return info_->isGenerator_; }
    size_t firstDefaultParam() const {
        return argCount() - (takesKeywords() ? 1 : 0) - defaults_.size();
    }
    Value paramDefault(unsigned i) {
        assert(i < info_->argCount());
        assert(i >= firstDefaultParam());
        return defaults_[i - firstDefaultParam()];
    }
    size_t argCount() const {
        return info_->argCount();
    }
    size_t defaultCount() const {
        return defaults_.size();
    }
    unsigned restParam() const {
        assert(takesRest());
        return info_->restParam_;
    }
    unsigned keywordsParam() const {
        assert(takesKeywords());
        return info_->keywordsParam_;
    }
    int findArg(Name name) {
        auto i = find(info_->params_.begin(), info_->params_.end(), name);
        if (i == info_->params_.end())
            return -1;
        return i - info_->params_.begin();
    }

    void traceChildren(Tracer& t) override;
    void dump(ostream& s) const override;

  private:
    Heap<FunctionInfo*> info_;
    HeapVector<Value> defaults_;
    Heap<Env*> env_;
};

struct Method : public Object
{
    static GlobalRoot<Class*> ObjectClass;

    Method(Traced<Callable*> callable, Traced<Object*> object);

    void traceChildren(Tracer& t) override;

    Callable* callable() { return callable_; }
    Object* object() { return object_; }

  private:
    Heap<Callable*> callable_;
    Heap<Object*> object_;
};

extern void initCallable();

#endif
