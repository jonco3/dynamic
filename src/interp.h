#ifndef __INTERP_H__
#define __INTERP_H__

#include "assert.h"
#include "block.h"
#include "common.h"
#include "frame.h"
#include "instr.h"
#include "token.h"
#include "value-inl.h"

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
        stack.push_back_reserved(value);
    }

    template <typename S1, typename S2>
    void pushStack(S1&& element1, S2&& element2) {
        Value values[2] = { Value(element1), Value(element2) };
        logStackPush(&values[0], values + 2);
        stack.push_back_reserved(values[0]);
        stack.push_back_reserved(values[1]);
    }

    template <typename S1, typename S2, typename S3>
    void pushStack(S1&& element1, S2&& element2, S3&& element3) {
        Value values[3] = { Value(element1), Value(element2), Value(element3) };
        logStackPush(&values[0], values + 3);
        stack.push_back_reserved(values[0]);
        stack.push_back_reserved(values[1]);
        stack.push_back_reserved(values[2]);
    }

    template <typename S>
    void fillStack(size_t count, S&& element) {
        Value value(element);
        for (size_t i = 0; i < count; i++)
            stack.push_back_reserved(value);
    }

    // Remove and return the value on the top of the stack.
    Value popStack() {
        logStackPop(1);
        Value result = stack.back();
        stack.pop_back();
        return result;
    }

    // Remove |count| values from the top of the stack.
    void popStack(size_t count) {
        logStackPop(count);
        for (size_t i = 0; i < count; i++)
            stack.pop_back();
    }

    // Return the value at position |offset| counting from zero.
    Value peekStack(size_t offset = 0) {
        return stack[stack.size() - offset - 1];
    }

    // Return a reference to the topmost stack value.
    MutableTraced<Value> refStack() {
        return stack.ref(stack.size() - 1);
    }

    // Return a reference to the |count| topmost values.
    TracedVector<Value> stackSlice(unsigned count, unsigned offsetFromTop = 0) {
        assert(stack.size() >= count + offsetFromTop);
        size_t pos = stack.size() - count - offsetFromTop;
        return TracedVector<Value>(stack, pos, count);
    }

    // Swap the two values on the top of the stack.
    void swapStack() {
        assert(stack.size() >= 2);
        swap(stack[stack.size() - 1], stack[stack.size() - 2]);
        logStackSwap();
    }

    void insertStackEntries(unsigned offsetFromTop, Value value,
                            size_t count = 1);
    void eraseStackEntries(unsigned offsetFromTop, size_t count);
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

    size_t stackPos() const {
        return stack.size();
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

    bool call(Traced<Value> callable, MutableTraced<Value> resultOut) {
        return syncCall(callable, 0, resultOut);
    }

    template <typename S>
    bool call(Traced<Value> callable,
              S&& arg1,
              MutableTraced<Value> resultOut)
    {
        Value value(arg1);
        logStackPush(value); stack.push_back(value);
        return syncCall(callable, 1, resultOut);
    }

    template <typename S1, typename S2>
    bool call(Traced<Value> callable,
              S1&& arg1, S2&& arg2,
              MutableTraced<Value> resultOut)
    {
        Value values[2] = { Value(arg1), Value(arg2) };
        logStackPush(&values[0], values + 2);
        stack.push_back(values[0]);
        stack.push_back(values[1]);
        return syncCall(callable, 2, resultOut);
    }

    template <typename S1, typename S2, typename S3>
    bool call(Traced<Value> callable,
              S1&& arg1, S2&& arg2, S3&& arg3,
              MutableTraced<Value> resultOut)
    {
        Value values[3] = { Value(arg1), Value(arg2), Value(arg3) };
        logStackPush(&values[0], values + 3);
        stack.push_back(values[0]);
        stack.push_back(values[1]);
        stack.push_back(values[2]);
        return syncCall(callable, 3, resultOut);
    }

    void startCall(Traced<Value> callable, unsigned posArgCount,
                   Traced<Layout*> keywordArgs = Layout::Empty,
                   unsigned extraPopCount = 0);

    void popFrame();

    size_t frameCount() const { return frames.size(); }
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
    void mungeSimpleArguments(Traced<Function*> function, unsigned argCount);
    bool mungeFullArguments(Traced<Function*> function,
                            Traced<Layout*> keywordArgs,
                            unsigned argCount,
                            MutableTraced<Value> resultOut);
    bool syncCall(Traced<Value> callable, NativeArgs args,
                  MutableTraced<Value> resultOut);
    bool syncCall(Traced<Value> callable, unsigned posArgCount,
                  MutableTraced<Value> resultOut);
    CallStatus setupCall(Traced<Value> target, unsigned argCount,
                         Traced<Layout*> keywordArgs, unsigned extraPopCount,
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

    template <typename T>
    void executeUnpackBuiltin(T* seq);

    void executeUnpackGeneric();
};

extern GlobalRoot<Interpreter*> interp;

#endif
