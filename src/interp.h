#ifndef __INTERP_H__
#define __INTERP_H__

#include "block.h"
#include "frame.h"
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

    bool exec(Traced<Block*> block, MutableTraced<Value> resultOut);

    template <typename S>
    void pushStack(S&& element) {
        Value value(element);
        logStackPush(value);
        stack.push_back(value);
    }

    template <typename S1, typename S2>
    void pushStack(S1&& element1, S2&& element2) {
        Value values[2] = { Value(element1), Value(element2) };
        logStackPush(&values[0], values + 2);
        stack.insert(stack.end(), &values[0], values + 2);
    }

    template <typename S1, typename S2, typename S3>
    void pushStack(S1&& element1, S2&& element2, S3&& element3) {
        Value values[3] = { Value(element1), Value(element2), Value(element3) };
        logStackPush(&values[0], values + 3);
        stack.insert(stack.end(), &values[0], values + 3);
    }

    // Remove and return the value on the top of the stack.
    Value popStack() {
        assert(!stack.empty());
        logStackPop(1);
        Value result = stack.back();
        stack.pop_back();
        return result;
    }

    // Remove |count| values from the top of the stack.
    void popStack(unsigned count) {
        assert(count <= stack.size());
        logStackPop(count);
        stack.resize(stack.size() - count);
    }

    // Return the value at position |offset| counting from zero.
    Value peekStack(unsigned offset) {
        assert(offset < stack.size());
        return stack[stackPos() - offset - 1];
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
        logStackSwap();
    }

    // Remove count stack entries located offsetFromTop entries from the top.
    void removeStackEntries(unsigned offsetFromTop, unsigned count);

    void insertStackEntry(unsigned offsetFromTop, Value value);

    Value getStackLocal(unsigned offset);
    void setStackLocal(unsigned offset, Traced<Value> value);

    GeneratorIter* getGeneratorIter();

    void branch(int offset);

    InstrThunk* nextInstr() { return instrp; }
    unsigned stackPos() { return stack.size(); }

    Env* env() { return getFrame()->env(); }
    Env* lexicalEnv(unsigned index);
    void setFrameEnv(Traced<Env*> env);

    bool raiseAttrError(Traced<Value> value, Name ident);
    bool raiseNameError(Name ident);

    bool call(Traced<Value> callable, unsigned argCount,
              MutableTraced<Value> resultOut);

    bool startCall(Traced<Value> callable, unsigned argCount);

    void popFrame();
    Frame* getFrame(unsigned reverseIndex = 0);
    void returnFromFrame(Value value);
#ifdef LOG_EXECUTION
    void logStart(int indentDelta = 0);
    void logStackPush(const Value& v);
    void logStackPush(const Value* first, const Value* last);
    void logStackPop(size_t count);
    void logStackSwap();
#else
    void logStackPush(const Value& v) {}
    void logStackPop(size_t count) {}
    void logStackPush(const Value* first, const Value* last) {}
    void logStackSwap() {}
#endif

    template <typename T>
    void replaceInstr(Instr* current, InstrFunc<T> newFunc, T* newData) {
        replaceInstr(current, reinterpret_cast<InstrFuncBase>(newFunc), newData);
    }

    template <typename T>
    void replaceInstrFunc(Instr* current, InstrFunc<T> newFunc) {
        replaceInstrFunc(current, reinterpret_cast<InstrFuncBase>(newFunc));
    }

    template <typename T>
    bool replaceInstrAndRestart(Instr* current, InstrFunc<T> newFunc, T* newData)
    {
        return replaceInstrAndRestart(current,
                                      reinterpret_cast<InstrFuncBase>(newFunc),
                                      newData);
    }

    template <typename T>
    bool replaceInstrFuncAndRestart(Instr* current, InstrFunc<T> newFunc) {
        return replaceInstrFuncAndRestart(current,
                                          reinterpret_cast<InstrFuncBase>(newFunc));
    }

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

    void pushExceptionHandler(ExceptionHandler::Type type, unsigned offset);
    void popExceptionHandler(ExceptionHandler::Type type);
    bool startExceptionHandler(Traced<Exception*> exception);
    bool isHandlingException() const;
    bool isHandlingDeferredReturn() const;
    bool isHandlingLoopControl() const;
    Exception* currentException();
    ExceptionHandler* currentExceptionHandler();
    bool maybeContinueHandlingException();
    void finishHandlingException();

  private:
    InstrThunk *instrp;
    RootVector<Frame> frames;
    RootVector<Value> stack;

    RootVector<ExceptionHandler*> exceptionHandlers;
    bool inExceptionHandler_;
    JumpKind jumpKind_;
    Stack<Exception*> currentException_;
    Stack<Value> deferredReturnValue_;
    unsigned remainingFinallyCount_;
    unsigned loopControlTarget_;

    unsigned frameIndex();
    void pushFrame(Traced<Block*> block, unsigned stackStartPos);
    unsigned currentOffset();
    TokenPos currentPos();

    bool run(MutableTraced<Value> resultOut);

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
    CallStatus setupCall(Traced<Value> target, TracedVector<Value> args,
                         MutableTraced<Value> resultOut);
    CallStatus setupCall(Traced<Value> target,
                         unsigned argCount,
                         MutableTraced<Value> resultOut);
    CallStatus raiseTypeError(string message, MutableTraced<Value> resultOut);

    void replaceInstr(Instr* current, InstrFuncBase newFunc, Instr* newData);
    void replaceInstrFunc(Instr* current, InstrFuncBase newFunc);
    bool replaceInstrAndRestart(Instr* current,
                                InstrFuncBase newFunc, Instr* newData);
    bool replaceInstrFuncAndRestart(Instr* current, InstrFuncBase newFunc);
};

extern Interpreter interp;

#endif
