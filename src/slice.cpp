#include "slice.h"

#include "exception.h"

#include "value-inl.h"

GlobalRoot<Class*> Slice::ObjectClass;
GlobalRoot<Layout*> Slice::InitialLayout;

void Slice::init()
{
    ObjectClass.init(Class::createNative<Slice>("slice"));

    Stack<Layout*> layout(Object::InitialLayout);
    layout = layout->addName(Names::start);
    layout = layout->addName(Names::stop);
    layout = layout->addName(Names::slice);
    assert(layout->lookupName(Names::start) == StartSlot);
    assert(layout->lookupName(Names::stop) == StopSlot);
    assert(layout->lookupName(Names::slice) == StepSlot);
    InitialLayout.init(layout);
}

#ifdef DEBUG
static bool IsInt32OrNone(Value value)
{
    return value == Value(None) || value.isInt();
}
#endif

Slice::Slice(TracedVector<Value> args) :
  Object(ObjectClass, InitialLayout)
{
    assert(args.size() == 3);
    assert(IsInt32OrNone(args[0]));
    assert(IsInt32OrNone(args[1]));
    assert(IsInt32OrNone(args[2]));
    setSlot(StartSlot, args[0]);
    setSlot(StopSlot, args[1]);
    setSlot(StepSlot, args[2]);
}

Slice::Slice(Traced<Class*> cls)
  : Object(cls)
{
    assert(cls->isDerivedFrom(ObjectClass));
}

int32_t Slice::getSlotOrDefault(unsigned slot, int32_t def)
{
    Value value = getSlot(slot);
    if (value == Value(None))
        return def;
    return value.asInt32();
}

void Slice::indices(int32_t length, int32_t& start, int32_t& stop, int32_t& step)
{
    // todo: should be a python method
    start = getSlotOrDefault(StartSlot, 0);
    stop = getSlotOrDefault(StopSlot, length);
    step = getSlotOrDefault(StepSlot, 1);
    if (step == 0)
        return;

    start = ClampIndex(WrapIndex(start, length), length, true);
    stop = ClampIndex(WrapIndex(stop, length), length, false);
}

bool Slice::getIterationData(int32_t length,
                             int32_t& startOut, int32_t& countOut,
                             int32_t& stepOut, MutableTraced<Value> resultOut)
{
    int32_t start, stop, step;
    indices(length, start, stop, step);
    if (step == 0) {
        resultOut = gc.create<ValueError>("slice step cannot be zero");
        return false;
    }

    int32_t step_sgn = step > 0 ? 1 : -1;
    startOut = start;
    countOut = max((stop - start + step - step_sgn) / step, 0);
    stepOut = step;
    return true;
}
