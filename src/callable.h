#ifndef __FUNCTION_H__
#define __FUNCTION_H__

#include "object.h"
#include "interp.h"
#include "block.h"

#include <memory>
#include <vector>

using namespace std;

struct Callable : public Object
{
    Callable(Class* cls) : Object(cls) {}
    virtual unsigned requiredArgs() = 0;
};

// todo: natives can't throw
struct Native : public Callable
{
    static void init();
    static Class* ObjectClass;
    Native() : Callable(ObjectClass) {}
    virtual bool call(Interpreter& interp) = 0;
};

struct Native0 : public Native
{
    typedef Value (*Func)();
    Native0(Func func) : func(func) {}
    virtual unsigned requiredArgs() { return 0; }
    virtual bool call(Interpreter& interp) {
        interp.pushStack(func());
        return true;
    }

  private:
    Func func;
};

struct Native1 : public Native
{
    typedef Value (*Func)(Value);
    Native1(Func func) : func(func) {}
    virtual unsigned requiredArgs() { return 1; }

    virtual bool call(Interpreter& interp) {
        Value arg = interp.popStack();
        interp.popStack();
        interp.pushStack(func(arg));
        return true;
    }

  private:
    Func func;
};

struct Native2 : public Native
{
    typedef Value (*Func)(Value, Value);
    Native2(Func func) : func(func) {}
    virtual unsigned requiredArgs() { return 2; }

    virtual bool call(Interpreter& interp) {
        Value arg2 = interp.popStack();
        Value arg1 = interp.popStack();
        interp.popStack();
        interp.pushStack(func(arg1, arg2));
        return true;
    }

  private:
    Func func;
};

struct Function : public Callable
{
    static void init();
    static Class* ObjectClass;

    Function(Block* block) : Callable(ObjectClass), block_(block) {}

    virtual unsigned requiredArgs() { return argNames.size(); }

    Name argName(unsigned i) {
        assert(i < argNames.size());
        return argNames[i];
    }

    Block* block() const { return block_.get(); }

  private:
    vector<Name> argNames;
    unique_ptr<Block> block_;
};


#endif
