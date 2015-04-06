#include "generator.h"

#include "callable.h"
#include "instr.h"

GlobalRoot<Class*> GeneratorIter::ObjectClass;

static bool generatorIter_iter(TracedVector<Value> args, Root<Value>& resultOut)
{
    GeneratorIter* i = args[0].toObject()->as<GeneratorIter>();
    return i->iter(resultOut);
}

void GeneratorIter::init()
{
    ObjectClass.init(gc::create<Class>("GeneratorIterator"));
    initNativeMethod(ObjectClass, "__iter__", 1, generatorIter_iter);

    Root<Layout*> layout(Frame::InitialLayout);
    layout = layout->addName("self");
    Root<Block*> block(gc::create<Block>(layout));
    block->append(gc::create<InstrResumeGenerator>());
    block->append(gc::create<InstrReturn>());
    static vector<Name> params = { "self" };
    Root<Frame*> scope; // todo: allow construction of traced for nullptr
    Root<Value> value(gc::create<Function>("next", params, block, scope));
    ObjectClass->setAttr("next", value);
}

GeneratorIter::GeneratorIter(Traced<Function*> func, Traced<Frame*> frame)
  : Object(ObjectClass),
    state_(Suspended),
    func_(func),
    frame_(frame),
    ipOffset_(0)
{
}

void GeneratorIter::traceChildren(Tracer& t)
{
    Object::traceChildren(t);
    gc::trace(t, &func_);
    gc::trace(t, &frame_);
    for (auto i = savedStack_.begin(); i != savedStack_.end(); i++)
        gc::trace(t, &*i);
}

bool GeneratorIter::iter(Root<Value>& resultOut)
{
    Root<GeneratorIter*> self(this);
    resultOut = self;
    return true;
}

bool GeneratorIter::resume(Interpreter& interp)
{
    switch (state_) {
      case Running:
        interp.pushStack(gc::create<Exception>("ValueError",
                                               "Generator running"));
        return false;

    case Suspended: {
        Root<Frame*> frame(frame_); // todo: Heap<T>
        interp.resumeGenerator(frame, ipOffset_, savedStack_);
        interp.pushStack(None);
        state_ = Running;
        return true;
    }

      case Finished:
        // todo: not sure about this behaviour, need tests
        interp.pushStack(gc::create<StopIteration>());
        return false;
    }
}

bool GeneratorIter::leave(Interpreter& interp)
{
    // todo: get rid of value
    assert(state_ == Running);
    interp.popFrame();
    state_ = Finished;
    interp.pushStack(gc::create<StopIteration>());
    return false;
}

void GeneratorIter::suspend(Interpreter& interp, Traced<Value> value)
{
    assert(state_ == Running);
    ipOffset_ = interp.suspendGenerator(savedStack_);
    state_ = Suspended;
    interp.pushStack(value);
}
