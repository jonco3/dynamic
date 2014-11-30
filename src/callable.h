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
    virtual bool call(Interpreter& interp) = 0;
};

struct Native0 : public Native
{
    typedef bool (*Func)(Root<Value>&);
    Native0(Func func) : func(func) {}
    virtual unsigned requiredArgs() { return 0; }
    virtual bool call(Interpreter& interp) {
        Root<Value> result;
        bool ok = func(result);
        interp.pushStack(result);
        return ok;
    }

  private:
    Func func;
};

struct Native1 : public Native
{
    typedef bool (*Func)(Traced<Value>, Root<Value>&);
    Native1(Func func) : func(func) {}
    virtual unsigned requiredArgs() { return 1; }

    virtual bool call(Interpreter& interp) {
        Root<Value> arg(interp.popStack());
        interp.popStack();
        Root<Value> result;
        bool ok = func(arg, result);
        interp.pushStack(result);
        return ok;
    }

  private:
    Func func;
};

struct Native2 : public Native
{
    typedef bool (*Func)(Traced<Value>, Traced<Value>, Root<Value>&);
    Native2(Func func) : func(func) {}
    virtual unsigned requiredArgs() { return 2; }

    virtual bool call(Interpreter& interp) {
        Root<Value> arg2(interp.popStack());
        Root<Value> arg1(interp.popStack());
        interp.popStack();
        Root<Value> result;
        bool ok = func(arg1, arg2, result);
        interp.pushStack(result);
        return ok;
    }

  private:
    Func func;
};

struct Function : public Callable
{
    static void init();
    static GlobalRoot<Class*> ObjectClass;

    Function(const vector<Name>& params, Traced<Block*> block, Traced<Frame*> scope)
      : Callable(ObjectClass), params_(params), block_(block), scope_(scope) {}

    virtual unsigned requiredArgs() { return params_.size(); }

    Name paramName(unsigned i) {
        assert(i < params_.size());
        return params_[i];
    }

    Block* block() const { return block_; }
    Frame* scope() const { return scope_; }

    virtual void traceChildren(Tracer& t) {
        gc::trace(t, &block_);
        gc::trace(t, &scope_);
    }

  private:
    const vector<Name>& params_;
    Block* block_;
    Frame* scope_;
};


#endif
