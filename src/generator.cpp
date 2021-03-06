#include "generator.h"

#include "callable.h"
#include "exception.h"
#include "frame.h"
#include "instr.h"
#include "interp.h"

//#define TRACE_GENERATOR

#ifdef TRACE_GENERATOR
static inline void log(const char* s, void *p, unsigned x) {
    cerr << s << " " << p << " " << x << endl;
}
#else
static inline void log(const char* s, void *p, unsigned x) {}
#endif

GlobalRoot<Class*> GeneratorIter::ObjectClass;

static bool generatorIter_iter(NativeArgs args, MutableTraced<Value> resultOut)
{
    GeneratorIter* i = args[0].as<GeneratorIter>();
    return i->iter(resultOut);
}

void GeneratorIter::init()
{
    ObjectClass.init(Class::createNative("GeneratorIterator", nullptr));
    initNativeMethod(ObjectClass, "__iter__", generatorIter_iter, 1);

    Stack<Env*> global;
    Stack<Layout*> layout(Env::InitialLayout);
    layout = layout->addName(Names::self);
    Stack<Block*> parent;
    Stack<Block*> block(gc.create<Block>(parent, global, layout, 1, true));
    block->append<Instr_CreateEnv>(); // todo: eventually we can remove this
    block->append<Instr_ResumeGenerator>();
    block->append<Instr_Return>();
    block->setMaxStackDepth(1);
    static vector<Name> params = { Names::self };
    Stack<FunctionInfo*> info(gc.create<FunctionInfo>(params, block));
    Stack<Value> value(gc.create<Function>(Names::__next__, info,
                                           EmptyValueArray));
    ObjectClass->setAttr(Names::__next__, value);
}

GeneratorIter::GeneratorIter(Traced<Block*> block,
                             Traced<Env*> env,
                             unsigned argCount)
  : Object(ObjectClass),
    state_(Running),
    block_(block),
    env_(env),
    exceptionHandlers_(nullptr),
    ipOffset_(0),
    argCount_(argCount)
{
    assert(savedStack_.empty());
}

void GeneratorIter::traceChildren(Tracer& t)
{
    Object::traceChildren(t);
    gc.trace(t, &block_);
    gc.trace(t, &env_);
    gc.trace(t, &exceptionHandlers_);
    gc.traceVector(t, &savedStack_);
}

bool GeneratorIter::iter(MutableTraced<Value> resultOut)
{
    Stack<GeneratorIter*> self(this);
    resultOut = Value(self);
    return true;
}

void GeneratorIter::resume(Interpreter& interp)
{
    log("GeneratorIter::resume", this, state_);
    switch (state_) {
      case Suspended: {
        interp.resumeGenerator(block_, env_, ipOffset_, savedStack_,
                               exceptionHandlers_);
        savedStack_.resize(0);
        exceptionHandlers_ = nullptr;
        if (state_ == Suspended)
            interp.pushStack(None);
        state_ = Running;
        assert(savedStack_.empty());
        break;
      }

      case Running:
        assert(savedStack_.empty());
        interp.raise<ValueError>("Generator running");
        break;

      case Finished:
        assert(savedStack_.empty());
        // todo: not sure about this behaviour, need tests
        interp.raise<StopIteration>();
        break;

      default:
        assert(false);
        exit(1);
    }
}

void GeneratorIter::leave(Interpreter& interp)
{
    log("GeneratorIter::leave", this, state_);
    assert(state_ == Running);
    assert(savedStack_.empty());
    interp.popFrame();
    state_ = Finished;
    Stack<Value> exception(gc.create<StopIteration>());
    interp.raiseException(exception);
}

void GeneratorIter::suspend(Interpreter& interp, Traced<Value> value)
{
    log("GeneratorIter::suspend", this, state_);
    assert(state_ == Running);
    assert(savedStack_.empty());
    Stack<ExceptionHandler*> ehs;
    ipOffset_ = interp.suspendGenerator(savedStack_, ehs);
    exceptionHandlers_ = ehs;
    state_ = Suspended;
    interp.pushStack(value);
}
