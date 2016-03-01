#ifndef __INTERP_H__
#define __INTERP_H__

#include "block.h"
#include "common.h"
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

    ExceptionHandler(Type type, unsigned stackPos, unsigned offset);

    ExceptionHandler* next() { return next_; }
    Type type() { return type_; }
    unsigned stackPos() { return stackPos_; }
    unsigned offset() { return offset_; }

    void setNext(ExceptionHandler* eh);

    void traceChildren(Tracer& t) override;

  protected:
    Heap<ExceptionHandler*> next_;
    const Type type_;
    unsigned stackPos_;
    unsigned offset_;
};

struct Interpreter : public Cell
{
    Interpreter();
    static void init();

    bool exec(Traced<Block*> block, MutableTraced<Value> resultOut);

    template <typename S>
    void pushStack(S&& element) {
        Value value(element);
        logStackPush(value);
        assert(stackPos_ + 1 <= stack.size());
        stack[stackPos_++] = value;
    }

    template <typename S1, typename S2>
    void pushStack(S1&& element1, S2&& element2) {
        Value values[2] = { Value(element1), Value(element2) };
        logStackPush(&values[0], values + 2);
        assert(stackPos_ + 2 <= stack.size());
        stack[stackPos_++] = values[0];
        stack[stackPos_++] = values[1];
    }

    template <typename S1, typename S2, typename S3>
    void pushStack(S1&& element1, S2&& element2, S3&& element3) {
        Value values[3] = { Value(element1), Value(element2), Value(element3) };
        logStackPush(&values[0], values + 3);
        assert(stackPos_ + 3 <= stack.size());
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
    Value peekStack(unsigned offset = 0) {
        assert(stackPos() >= offset + 1);
        return stack[stackPos_ - offset - 1];
    }

    // Return a reference to the topmost stack value.
    MutableTraced<Value> refStack() {
        return stack.ref(stackPos_ - 1);
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

    void insertStackEntry(unsigned offsetFromTop, Value value);

    void ensureStackSpace(size_t newStackSize);

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

    template <typename T>
    void raise() {
        pushStack(gc.create<T>());
        raiseException();
    }

    template <typename T>
    void raise(const string& message) {
        pushStack(gc.create<T>(message));
        raiseException();
    }

    void raiseAttrError(Traced<Value> value, Name ident);
    void raiseNameError(Name ident);

    bool call(Traced<Value> callable, unsigned argCount,
              MutableTraced<Value> resultOut);

    void startCall(Traced<Value> callable,
                   unsigned argCount, unsigned extraPopCount = 0);

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

    const Heap<Instr*>& currentInstr() const;
    void insertStubInstr(Instr* current, Instr* stub);
    void replaceAllStubs(Instr* current, Instr* stub);

    void resumeGenerator(Traced<Block*> block,
                         Traced<Env*> env,
                         unsigned ipOffset,
                         TracedVector<Value> savedStack,
                         Traced<ExceptionHandler*> savedHandlers);
    unsigned suspendGenerator(HeapVector<Value>& savedStack,
                              MutableTraced<ExceptionHandler*> savedHandlers);

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
    void maybeContinueHandlingException();
    void finishHandlingException();

  private:
    static GlobalRoot<Block*> AbortTrampoline;

    InstrThunk *instrp;
    HeapVector<Frame, std::vector<Frame>> frames;
    // todo: why is std::vector significantly faster?
    HeapVector<Value> stack;
    unsigned stackPos_;

    bool inExceptionHandler_;
    JumpKind jumpKind_;
    Heap<Exception*> currentException_;
    Heap<Value> deferredReturnValue_;
    unsigned remainingFinallyCount_;
    unsigned loopControlTarget_;

    void traceChildren(Tracer& t) override;

    unsigned frameIndex() {
        assert(!frames.empty());
        return frames.size() - 1;
    }

    void pushFrame(Traced<Block*> block, unsigned stackStartPos,
                   unsigned extraPopCount);
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

    bool failWithArgumentError(Traced<Callable*> callable,
                               unsigned count,
                               MutableTraced<Value> resultOut);
    CallStatus failWithTypeError(string message,
                                 MutableTraced<Value> resultOut);
    bool checkArguments(Traced<Callable*> callable,
                        const TracedVector<Value>& args,
                        MutableTraced<Value> resultOut);
    unsigned mungeArguments(Traced<Function*> function, unsigned argCount);
    bool call(Traced<Value> callable, NativeArgs args,
              MutableTraced<Value> resultOut);
    CallStatus setupCall(Traced<Value> target,
                         unsigned argCount, unsigned extraPopCount,
                         MutableTraced<Value> resultOut);

    // Instruction implementations
#define declare_instr_method(name, cls)                                       \
    void executeInstr_##name(Traced<cls*> self);

    for_each_outofline_instr(declare_instr_method)
#undef declare_instr_method

    template <BinaryOp Op>
    void executeBinaryOpInt(Traced<BinaryOpStubInstr*> instr);

    template <BinaryOp Op>
    void executeBinaryOpFloat(Traced<BinaryOpStubInstr*> instr);

    template <CompareOp Op>
    void executeCompareOpInt(Traced<CompareOpStubInstr*> instr);

    template <CompareOp Op>
    void executeCompareOpFloat(Traced<CompareOpStubInstr*> instr);

    template <BinaryOp Op>
    void executeAugAssignUpdateInt(Traced<BinaryOpStubInstr*> instr);

    template <BinaryOp Op>
    void executeAugAssignUpdateFloat(Traced<BinaryOpStubInstr*> instr);

    bool maybeCallBinaryOp(Traced<Value> obj, Name name,
                           Traced<Value> left, Traced<Value> right,
                           MutableTraced<Value> methodOut, bool& okOut);
    bool executeBinaryOp(BinaryOp op, MutableTraced<Value> methodOut);
    bool executeCompareOp(CompareOp op, MutableTraced<Value> methodOut);
    bool executeAugAssignUpdate(BinaryOp op, StackMethodAttr& method);
    bool getIterator(MutableTraced<Value> resultOut);

    template <typename T>
    void executeDestructureBuiltin(unsigned count, T* seq);

    void executeDestructureGeneric(unsigned count);
};

extern GlobalRoot<Interpreter*> interp;

#endif
