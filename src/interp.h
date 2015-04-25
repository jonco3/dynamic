#ifndef __INTERP_H__
#define __INTERP_H__

#include "frame.h"
#include "value-inl.h"

#include <cassert>
#include <vector>

using namespace std;

struct Block;
struct Callable;
struct Function;
struct Instr;

struct Interpreter
{
    static void init();
    static bool exec(Traced<Block*> block, Value& valueOut);
    static Interpreter& instance() { return *instance_; }

    bool interpret(Traced<Block*> block, Value& valueOut);

    template <typename S>
    void pushStack(const S& element) {
        Root<Value> value(element);
        stack.resize(pos + 1);
        stack[pos] = value.get();
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

    bool call(Traced<Value> callable, const TracedVector<Value>& args,
              Root<Value>& resultOut);
    bool startCall(Traced<Value> callable, const TracedVector<Value>& args);

    void popFrame();
    Frame* getFrame(unsigned reverseIndex = 0);

    void replaceInstr(Instr* current, Instr* newInstr);
    bool replaceInstrAndRestart(Instr* current, Instr* newInstr);

    void resumeGenerator(Traced<Frame*> frame,
                         unsigned ipOffset,
                         vector<Value>& savedStack);
    unsigned suspendGenerator(vector<Value>& savedStackx);

  private:
    static Interpreter* instance_;

    Instr **instrp;
    RootVector<Frame*> frames;
    RootVector<Value> stack;
    unsigned pos;

    Interpreter();
    Frame* newFrame(Traced<Function*> function);
    void pushFrame(Traced<Frame*> frame);

    bool run(Value& valueOut);

    enum CallStatus {
        CallError,
        CallStarted,
        CallFinished
    };

    bool checkArguments(Traced<Callable*> callable,
                        const TracedVector<Value>& args,
                        Root<Value>& resultOut);
    CallStatus setupCall(Traced<Value> target, const TracedVector<Value>& args,
                         Root<Value>& resultOut);
    CallStatus raise(string className, string message, Root<Value>& resultOut);
};

#ifdef BUILD_TESTS
extern void testInterp(const string& input, const string& expected);
extern void testException(const string& input, const string& expected);
#endif

#endif
