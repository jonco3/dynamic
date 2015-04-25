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
        stack.push_back(value.get());
    }

    // Remove and return the value on the top of the stack.
    Value popStack() {
        assert(!stack.empty());
        Value result = stack.back();
        stack.pop_back();
        return result;
    }

    // Remove |count| values from the top of the stack.
    void popStack(unsigned count) {
        assert(count <= stack.size());
        stack.resize(stack.size() - count);
    }

    // Return the value at position |offset| counting from zero.
    Value peekStack(unsigned offset) {
        assert(offset < stack.size());
        return stack[stackPos() - offset - 1];
    }

    // Get a reference to the value at position |offset| counting from zero.
    Traced<Value> stackRef(unsigned offset) {
        assert(offset < stack.size());
        return stack.ref(stackPos() - offset - 1);
    }

    // Return a reference to the |count| topmost values.
    TracedVector<Value> stackSlice(unsigned count) {
        assert(count <= stack.size());
        return TracedVector<Value>(stack, stackPos() - count, count);
    }

    // Swap the two values on the top of the stack.
    void swapStack() {
        assert(stack.size() >= 2);
        unsigned pos = stackPos();
        swap(stack[pos - 1], stack[pos - 2]);
    }

    void branch(int offset);

    Instr** nextInstr() { return instrp; }
    unsigned stackPos() { return stack.size(); }

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
