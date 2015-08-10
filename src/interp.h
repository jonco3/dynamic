#ifndef __INTERP_H__
#define __INTERP_H__

#include "block.h"
#include "frame.h"
#include "instr.h"
#include "token.h"
#include "value-inl.h"

#include <cassert>
#include <vector>

using namespace std;

#ifdef DEBUG
#define LOG_EXECUTION
#endif

#ifdef LOG_EXECUTION
extern bool logFrames;
extern bool logExecution;
#endif

struct Callable;
struct Exception;
struct ExceptionHandler;
struct Function;
struct GeneratorIter;

struct ExceptionHandler : public Cell
{
    enum Type {
        CatchHandler, FinallyHandler
    };

    ExceptionHandler(Type type, unsigned frameIndex, unsigned offset);

    Type type() { return type_; }
    unsigned frameIndex() { return frameIndex_; }
    unsigned offset() { return offset_; }

  protected:
    const Type type_;
    unsigned frameIndex_;
    unsigned offset_;
};

struct Interpreter
{
    Interpreter();
    static void init();

    bool exec(Traced<Block*> block, MutableTraced<Value> resultOut);

    template <typename S>
    void pushStack(S&& element) {
        assert(stackPos_ + 1 <= stack.size());
        Value value(element);
        logStackPush(value);
        stack[stackPos_++] = value;
    }

    template <typename S1, typename S2>
    void pushStack(S1&& element1, S2&& element2) {
        assert(stackPos_ + 2 <= stack.size());
        Value values[2] = { Value(element1), Value(element2) };
        logStackPush(&values[0], values + 2);
        stack[stackPos_++] = values[0];
        stack[stackPos_++] = values[1];
    }

    template <typename S1, typename S2, typename S3>
    void pushStack(S1&& element1, S2&& element2, S3&& element3) {
        assert(stackPos_ + 3 <= stack.size());
        Value values[3] = { Value(element1), Value(element2), Value(element3) };
        logStackPush(&values[0], values + 3);
        stack[stackPos_++] = values[0];
        stack[stackPos_++] = values[1];
        stack[stackPos_++] = values[2];
    }

    template <typename S>
    void fillStack(unsigned count, S&& element) {
        assert(stackPos_ + count <= stack.size());
        Value value(element);
        for (unsigned i = 0; i < count; i++)
            stack[stackPos_++] = value;
    }

    // Remove and return the value on the top of the stack.
    Value popStack() {
        assert(stackPos() >= 1);
        logStackPop(1);
        Value result = stack[--stackPos_];
        stack[stackPos_] = Value(None);
        return result;
    }

    // Remove |count| values from the top of the stack.
    void popStack(unsigned count) {
        assert(stackPos() >= count);
        logStackPop(count);
        stackPos_ -= count;
        for (unsigned i = 0; i < count; i++)
            stack[stackPos_ + i] = Value(None);
    }

    // Return the value at position |offset| counting from zero.
    Value peekStack(unsigned offset) {
        assert(stackPos() >= offset + 1);
        return stack[stackPos_ - offset - 1];
    }

    // Return a reference to the |count| topmost values.
    TracedVector<Value> stackSlice(unsigned count) {
        assert(stackPos() >= count);
        return TracedVector<Value>(stack, stackPos_ - count, count);
    }

    // Swap the two values on the top of the stack.
    void swapStack() {
        assert(stackPos() >= 2);
        swap(stack[stackPos_ - 1], stack[stackPos_ - 2]);
        logStackSwap();
    }

    // Remove count stack entries located offsetFromTop entries from the top.
    void removeStackEntries(unsigned offsetFromTop, unsigned count);

    void insertStackEntry(unsigned offsetFromTop, Value value);

    Value getStackLocal(unsigned offset) {
        Frame* frame = getFrame();
        assert(offset < frame->block()->layout()->slotCount());
        return stack[frame->stackPos() + offset];
    }

    template <typename S>
    void setStackLocal(unsigned offset, S&& element) {
        Frame* frame = getFrame();
        assert(offset < frame->block()->layout()->slotCount());
        stack[frame->stackPos() + offset] = Value(element);
    }

    GeneratorIter* getGeneratorIter();

    void branch(int offset);

    InstrThunk* nextInstr() { return instrp; }

    unsigned stackPos() const {
        assert(stackPos_ <= stack.size());
        return stackPos_;
    }

    Env* env() { return getFrame()->env(); }
    Env* lexicalEnv(unsigned index);
    void setFrameEnv(Env* env);

    bool raiseAttrError(Traced<Value> value, Name ident);
    bool raiseNameError(Name ident);

    bool call(Traced<Value> callable, unsigned argCount,
              MutableTraced<Value> resultOut);

    void startCall(Traced<Value> callable, unsigned argCount);

    void popFrame();

    Frame* getFrame(unsigned reverseIndex = 0) {
        assert(reverseIndex < frames.size());
        return &frames[frameIndex() - reverseIndex];
    }

    void returnFromFrame(Value value);
#ifdef LOG_EXECUTION
    void logStart(int indentDelta = 0);
    void logStackPush(const Value& v);
    void logStackPush(const Value* first, const Value* last);
    void logStackPop(size_t count);
    void logStackSwap();
    void logInstr(Instr* instr);
#else
    void logStackPush(const Value& v) {}
    void logStackPop(size_t count) {}
    void logStackPush(const Value* first, const Value* last) {}
    void logStackSwap() {}
    void logInstr(Instr* instr) {}
#endif

    void replaceInstr(Instr* current, Instr* newData);
    void replaceInstrAndRestart(Instr* current, Instr* newData);

    void resumeGenerator(Traced<Block*> block,
                         Traced<Env*> env,
                         unsigned ipOffset,
                         vector<Value>& savedStack);
    unsigned suspendGenerator(vector<Value>& savedStackx);

    void loopControlJump(unsigned finallyCount, unsigned target);

    enum class JumpKind {
        None,
        Exception,
        Return,
        LoopControl
    };

    void raiseException(Traced<Value> exception);
    void raiseException();
    void pushExceptionHandler(ExceptionHandler::Type type, unsigned offset);
    void popExceptionHandler(ExceptionHandler::Type type);
    bool isHandlingException() const;
    bool isHandlingDeferredReturn() const;
    bool isHandlingLoopControl() const;
    Exception* currentException();
    ExceptionHandler* currentExceptionHandler();
    void maybeContinueHandlingException();
    void finishHandlingException();

  private:
    static GlobalRoot<Block*> AbortTrampoline;

    InstrThunk *instrp;
    RootVector<Frame> frames;
    RootVector<Value> stack;
    unsigned stackPos_;

    RootVector<ExceptionHandler*> exceptionHandlers;
    bool inExceptionHandler_;
    JumpKind jumpKind_;
    Stack<Exception*> currentException_;
    Stack<Value> deferredReturnValue_;
    unsigned remainingFinallyCount_;
    unsigned loopControlTarget_;

    unsigned frameIndex() {
        assert(!frames.empty());
        return frames.size() - 1;
    }

    void pushFrame(Traced<Block*> block, unsigned stackStartPos);
    unsigned currentOffset();
    TokenPos currentPos();

    bool run(MutableTraced<Value> resultOut);
    bool handleException();
    bool startExceptionHandler(Traced<Exception*> exception);
    bool startNextFinallySuite(JumpKind jumpKind);

    enum CallStatus {
        CallError,
        CallStarted,
        CallFinished
    };

    bool checkArguments(Traced<Callable*> callable,
                        const TracedVector<Value>& args,
                        MutableTraced<Value> resultOut);
    unsigned mungeArguments(Traced<Function*> function, unsigned argCount);
    bool call(Traced<Value> callable, TracedVector<Value> args,
              MutableTraced<Value> resultOut);
    CallStatus setupCall(Traced<Value> target,
                         unsigned argCount,
                         MutableTraced<Value> resultOut);
    CallStatus raiseTypeError(string message, MutableTraced<Value> resultOut);

    // Instruction implementations
#define declare_instr_method(name)                                            \
    void executeInstr_##name(Traced<Instr##name*> self);
    for_each_outofline_instr(declare_instr_method)
#undef declare_instr_method

    template <BinaryOp Op>
    void executeBinaryOpInt(Traced<InstrBinaryOpInt<Op>*> instr);

    template <CompareOp Op>
    void executeCompareOpInt(Traced<InstrCompareOpInt<Op>*> instr);
};

extern Interpreter interp;

#endif
