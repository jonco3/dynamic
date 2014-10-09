#ifndef __INTERP_H__
#define __INTERP_H__

#include "value.h"
#include "repr.h"

#include <cassert>
#include <vector>
#include <iostream>

using namespace std;

struct Instr;
struct Frame;
struct Block;
struct Function;

struct Interpreter
{
    Interpreter() : instrp(nullptr), frame(nullptr) {}

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

    Frame* pushFrame(Function* function);
    void popFrame();

  private:
    Instr **instrp;
    Frame *frame;
    vector<Value> stack;
};

#endif
