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
    Callable(Traced<Class*> cls) : Object(cls) {}
    virtual unsigned requiredArgs() = 0;
};

struct Native : public Callable
{
    static void init();
    static GlobalRoot<Class*> ObjectClass;
    Native() : Callable(ObjectClass) {}
    virtual bool call(TracedVector<Value> args, Root<Value>& resultOut) = 0;
};

struct Native0 : public Native
{
    typedef bool (*Func)(Root<Value>&);
    Native0(Func func) : func(func) {}
    virtual unsigned requiredArgs() { return 0; }
    virtual bool call(TracedVector<Value> args, Root<Value>& resultOut) {
        assert(args.size() == 0);
        return func(resultOut);
    }

  private:
    Func func;
};

struct Native1 : public Native
{
    typedef bool (*Func)(Traced<Value>, Root<Value>&);
    Native1(Func func) : func(func) {}
    virtual unsigned requiredArgs() { return 1; }

    virtual bool call(TracedVector<Value> args, Root<Value>& resultOut) {
        assert(args.size() == 1);
        return func(args[0], resultOut);
    }

  private:
    Func func;
};

struct Native2 : public Native
{
    typedef bool (*Func)(Traced<Value>, Traced<Value>, Root<Value>&);
    Native2(Func func) : func(func) {}
    virtual unsigned requiredArgs() { return 2; }

    virtual bool call(TracedVector<Value> args, Root<Value>& resultOut) {
        assert(args.size() == 2);
        return func(args[0], args[1], resultOut);
    }

  private:
    Func func;
};

struct Native3 : public Native
{
    typedef bool (*Func)(Traced<Value>, Traced<Value>, Traced<Value>, Root<Value>&);
    Native3(Func func) : func(func) {}
    virtual unsigned requiredArgs() { return 3; }

    virtual bool call(TracedVector<Value> args, Root<Value>& resultOut) {
        assert(args.size() == 3);
        return func(args[0], args[1], args[2], resultOut);
    }

  private:
    Func func;
};

struct Function : public Callable
{
    static void init();
    static GlobalRoot<Class*> ObjectClass;

    Function(const vector<Name>& params,
             Traced<Block*> block,
             Traced<Frame*> scope,
             bool isGenerator = false);

    virtual unsigned requiredArgs() { return params_.size(); }

    Name paramName(unsigned i) {
        assert(i < params_.size());
        return params_[i];
    }

    Block* block() const { return block_; }
    Frame* scope() const { return scope_; }
    bool isGenerator() { return isGenerator_; }

    virtual void traceChildren(Tracer& t) {
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
