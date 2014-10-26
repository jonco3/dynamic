#ifndef __INTERP_H__
#define __INTERP_H__

#include "frame.h"
#include "repr.h"
#include "value-inl.h"

#include <cassert>
#include <vector>

using namespace std;

struct Instr;
struct Block;
struct Function;

struct Interpreter
{
    Interpreter();

    bool interpret(Block* block, Value& valueOut);

    void pushStack(Value value) {
        stack.push_back(value);
    }

    Value popStack() {
        assert(!stack.empty());
        Value result = stack[stack.size() - 1];
        stack.pop_back();
        return result;
    }

    Value peekStack(unsigned pos) {
        assert(stack.size() >= pos + 1);
        return stack[stack.size() - pos - 1];
    }

    void swapStack() {
        assert(stack.size() >= 2);
        swap(stack[stack.size() - 1], stack[stack.size() - 2]);
    }

    void branch(int offset);

    Instr** nextInstr() { return instrp; }
    unsigned stackPos() { return stack.size(); }

    Frame* newFrame(Function* function);
    void pushFrame(Frame* frame);
    void popFrame();
    Frame* getFrame(unsigned reverseIndex = 0);

  private:
    Instr **instrp;
    RootVector<Frame*> frames;
    RootVector<Value> stack;
};

#endif
