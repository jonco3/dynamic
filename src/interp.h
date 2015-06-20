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

struct ExceptionHandler : public Cell
{
    enum Type {
        CatchHandler, FinallyHandler
    };

    ExceptionHandler(Type type, Traced<Frame*> frame, unsigned offset);
    virtual void traceChildren(Tracer& t) override;

    Type type() { return type_; }
    Frame* frame() { return frame_; }
    unsigned offset() { return offset_; }

  protected:
    const Type type_;
    Frame* frame_;
    unsigned offset_;
};

struct Interpreter
{
    static void init();
    static bool exec(Traced<Block*> block, MutableTraced<Value> resultOut);
    static Interpreter& instance() { return *instance_; }

    bool interpret(Traced<Block*> block, MutableTraced<Value> resultOut);

    template <typename S>
    void pushStack(const S& element) {
        Stack<Value> value(element);
        stack.push_back(value.get());
        logStackPush(value);
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
        logStackSwap();
    }

    void branch(int offset);

    InstrThunk* nextInstr() { return instrp; }
    unsigned stackPos() { return stack.size(); }

    bool raiseAttrError(Traced<Value> value, Name ident);
    bool raiseNameError(Name ident);

    bool call(Traced<Value> callable, const TracedVector<Value>& args,
              MutableTraced<Value> resultOut);
    bool startCall(Traced<Value> callable, const TracedVector<Value>& args);

    void popFrame();
    Frame* getFrame(unsigned reverseIndex = 0);
    void returnFromFrame(Value value);
#ifdef DEBUG
    unsigned frameStackDepth();
#endif
#ifdef LOG_EXECUTION
    void logStart(int indentDelta = 0);
    void logStackPush(const Value& v);
    void logStackPop(size_t count);
    void logStackSwap();
#else
    void logStackPush(const Value& v) {}
    void logStackPop(size_t count) {}
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

    void resumeGenerator(Traced<Frame*> frame,
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
    static Interpreter* instance_;

    InstrThunk *instrp;
    RootVector<Frame*> frames;
    RootVector<Value> stack;

    RootVector<ExceptionHandler*> exceptionHandlers;
    bool inExceptionHandler_;
    JumpKind jumpKind_;
    Stack<Exception*> currentException_;
    Stack<Value> deferredReturnValue_;
    unsigned remainingFinallyCount_;
    unsigned loopControlTarget_;

    Interpreter();
    Frame* newFrame(Traced<Function*> function);
    void pushFrame(Traced<Frame*> frame);
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
    CallStatus setupCall(Traced<Value> target, const TracedVector<Value>& args,
                         MutableTraced<Value> resultOut);
    CallStatus raiseTypeError(string message, MutableTraced<Value> resultOut);

    void replaceInstr(Instr* current, InstrFuncBase newFunc, Instr* newData);
    void replaceInstrFunc(Instr* current, InstrFuncBase newFunc);
    bool replaceInstrAndRestart(Instr* current,
                                InstrFuncBase newFunc, Instr* newData);
    bool replaceInstrFuncAndRestart(Instr* current, InstrFuncBase newFunc);
};

#endif
