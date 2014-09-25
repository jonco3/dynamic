#ifndef __FUNCTION_H__
#define __FUNCTION_H__

#include "object.h"
#include "interp.h"
#include "block.h"

#include <vector>

using namespace std;

struct Callable : public Object
{
    Callable(Class* cls) : Object(cls) {}
    virtual unsigned requiredArgs() = 0;
};

struct Native : public Callable
{
    static Class Class;
    Native() : Callable(&Class) {}
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
        interp.pushStack(func(arg1, arg2));
        return true;
    }

  private:
    Func func;
};

struct Function : public Callable
{
    static Class Class;

    Function() : Callable(&Class) {}

    virtual unsigned requiredArgs() { return argNames.size(); }
    Instr** startInstr() { return block->startInstr(); }

    Name argName(unsigned i) {
        assert(i < argNames.size());
        return argNames[i];
    }

  private:
    vector<Name> argNames;
    auto_ptr<Block> block;
};


#endif
