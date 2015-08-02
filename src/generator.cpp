#include "generator.h"

#include "callable.h"
#include "instr.h"

//#define TRACE_GENERATOR

#ifdef TRACE_GENERATOR
static inline void log(const char* s, void *p, unsigned x) {
    cerr << s << " " << p << " " << x << endl;
}
#else
static inline void log(const char* s, void *p, unsigned x) {}
#endif

GlobalRoot<Class*> GeneratorIter::ObjectClass;

static bool generatorIter_iter(TracedVector<Value> args, MutableTraced<Value> resultOut)
{
    GeneratorIter* i = args[0].toObject()->as<GeneratorIter>();
    return i->iter(resultOut);
}

void GeneratorIter::init()
{
    ObjectClass.init(Class::createNative("GeneratorIterator", nullptr));
    initNativeMethod(ObjectClass, "__iter__", generatorIter_iter, 1);

    Stack<Layout*> layout(Env::InitialLayout);
    layout = layout->addName("self");
    Stack<Block*> block(gc.create<Block>(layout, 1, true));
    block->append<InstrCreateEnv>(); // todo: eventually we can remove this
    block->append<InstrResumeGenerator>(); // never returns
    static vector<Name> params = { "self" };
    Stack<Env*> env; // todo: allow construction of traced for nullptr
    Stack<FunctionInfo*> info(gc.create<FunctionInfo>(params, block));
    Stack<Value> value(gc.create<Function>("next", info, EmptyValueArray, env));
    ObjectClass->setAttr("next", value);
}

GeneratorIter::GeneratorIter(Traced<Block*> block,
                             Traced<Env*> env,
                             unsigned argCount)
  : Object(ObjectClass),
    state_(Running),
    block_(block),
    env_(env),
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
    for (auto i = savedStack_.begin(); i != savedStack_.end(); i++)
        gc.trace(t, &*i);
}

bool GeneratorIter::iter(MutableTraced<Value> resultOut)
{
    Stack<GeneratorIter*> self(this);
    resultOut = Value(self);
    return true;
}

bool GeneratorIter::resume(Interpreter& interp)
{
    log("GeneratorIter::resume", this, state_);
    switch (state_) {
      case Suspended: {
        Stack<Env*> env(env_); // todo: Heap<T>
        Stack<Block*> block(block_);
        interp.resumeGenerator(block, env, ipOffset_, savedStack_);
        if (state_ == Suspended)
            interp.pushStack(None);
        state_ = Running;
        assert(savedStack_.empty());
        return true;
      }

      case Running:
        assert(savedStack_.empty());
        interp.pushStack(gc.create<ValueError>("Generator running"));
        return false;

      case Finished:
        assert(savedStack_.empty());
        // todo: not sure about this behaviour, need tests
        interp.pushStack(gc.create<StopIteration>());
        return false;

      default:
        assert(false);
        exit(1);
    }
}

bool GeneratorIter::leave(Interpreter& interp)
{
    log("GeneratorIter::leave", this, state_);
    assert(state_ == Running);
    assert(savedStack_.empty());
    interp.popFrame();
    state_ = Finished;
    interp.pushStack(gc.create<StopIteration>());
    return false;
}

void GeneratorIter::suspend(Interpreter& interp, Traced<Value> value)
{
    log("GeneratorIter::suspend", this, state_);
    assert(state_ == Running);
    assert(savedStack_.empty());
    ipOffset_ = interp.suspendGenerator(savedStack_);
    state_ = Suspended;
    interp.pushStack(value);
}
