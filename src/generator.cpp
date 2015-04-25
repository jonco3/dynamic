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

static bool generatorIter_iter(TracedVector<Value> args, Root<Value>& resultOut)
{
    GeneratorIter* i = args[0].toObject()->as<GeneratorIter>();
    return i->iter(resultOut);
}

void GeneratorIter::init()
{
    ObjectClass.init(gc.create<Class>("GeneratorIterator"));
    initNativeMethod(ObjectClass, "__iter__", generatorIter_iter, 1);

    Root<Layout*> layout(Frame::InitialLayout);
    layout = layout->addName("self");
    Root<Block*> block(gc.create<Block>(layout));
    block->append(gc.create<InstrResumeGenerator>());
    block->append(gc.create<InstrReturn>());
    static vector<Name> params = { "self" };
    Root<Frame*> scope; // todo: allow construction of traced for nullptr
    RootVector<Value> defaults; // todo: find a way of passing an empty vector
    Root<FunctionInfo*> info(gc.create<FunctionInfo>(params, block));
    Root<Value> value(gc.create<Function>("next", info, defaults, scope));
    ObjectClass->setAttr("next", value);
}

GeneratorIter::GeneratorIter(Traced<Function*> func, Traced<Frame*> frame)
  : Object(ObjectClass),
    state_(Suspended),
    func_(func),
    frame_(frame),
    ipOffset_(0)
{
    assert(savedStack_.empty());
}

void GeneratorIter::traceChildren(Tracer& t)
{
    Object::traceChildren(t);
    gc.trace(t, &func_);
    gc.trace(t, &frame_);
    for (auto i = savedStack_.begin(); i != savedStack_.end(); i++)
        gc.trace(t, &*i);
}

bool GeneratorIter::iter(Root<Value>& resultOut)
{
    Root<GeneratorIter*> self(this);
    resultOut = self;
    return true;
}

bool GeneratorIter::resume(Interpreter& interp)
{
    log("GeneratorIter::resume", this, state_);
    switch (state_) {
      case Running:
        assert(savedStack_.empty());
        interp.pushStack(gc.create<Exception>("ValueError",
                                               "Generator running"));
        return false;

      case Suspended: {
        Root<Frame*> frame(frame_); // todo: Heap<T>
        interp.resumeGenerator(frame, ipOffset_, savedStack_);
        interp.pushStack(None);
        state_ = Running;
        assert(savedStack_.empty());
        return true;
      }

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
