#ifndef __INTERP_H__
#define __INTERP_H__

#include "frame.h"
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

    bool interpret(Traced<Block*> block, Value& valueOut);

    void pushStack(Value value) {
        stack.resize(pos + 1);
        stack[pos] = value;
        ++pos;
    }

    Value popStack() {
        assert(pos >= 1);
        assert(pos <= stack.size());
        --pos;
        Value result = stack[pos];
        return result;
    }

    void popStack(unsigned count) {
        assert(pos >= count);
        assert(pos <= stack.size());
        pos -= count;
    }

    Value peekStack(unsigned offset) {
        assert(pos >= offset + 1);
        assert(pos <= stack.size());
        return stack[pos - offset - 1];
    }

    Traced<Value> stackRef(unsigned offset) {
        assert(pos >= offset + 1);
        assert(pos <= stack.size());
        return stack.ref(pos - offset - 1);
    }

    TracedVector<Value> stackSlice(unsigned offset, unsigned size) {
        assert(pos >= offset + 1);
        assert(pos <= stack.size());
        return TracedVector<Value>(stack, pos - offset - 1, size);
    }

    void swapStack() {
        assert(pos >= 2);
        assert(pos <= stack.size());
        swap(stack[pos - 1], stack[pos - 2]);
    }

    void branch(int offset);

    Instr** nextInstr() { return instrp; }
    unsigned stackPos() { return pos; }

    bool startCall(Traced<Value> target, const TracedVector<Value>& args);

    void popFrame();
    Frame* getFrame(unsigned reverseIndex = 0);

    void replaceInstr(Instr* instr, Instr* prev);

  private:
    Instr **instrp;
    RootVector<Frame*> frames;
    RootVector<Value> stack;
    unsigned pos;

    bool raise(string message);
    Frame* newFrame(Traced<Function*> function);
    void pushFrame(Traced<Frame*> frame);
};

#ifdef BUILD_TESTS
extern void testInterp(const string& input, const string& expected);
extern void testException(const string& input, const string& expected);
#endif

#endif
