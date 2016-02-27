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

Slice::Slice(NativeArgs args) :
  ObjectInline<3>(ObjectClass, InitialLayout)
{
    assert(args.size() == 3);
    assert(IsInt32OrNone(args[0]));
    assert(IsInt32OrNone(args[1]));
    assert(IsInt32OrNone(args[2]));
    setSlot(StartSlot, args[0]);
    setSlot(StopSlot, args[1]);
    setSlot(StepSlot, args[2]);
    assert(!hasOutOfLineSlots());
}

Slice::Slice(Traced<Class*> cls)
  : ObjectInline<3>(cls)
{
    assert(cls->isDerivedFrom(ObjectClass));
}

bool Slice::getSlotIfNotNone(unsigned slot, int32_t& resultOut)
{
    Value value = getSlot(slot);
    if (value == Value(None))
        return false;

    resultOut = value.asInt32();
    return true;
}

void Slice::indices(int32_t length, int32_t& start, int32_t& stop, int32_t& step)
{
    // todo: should be a python method
    if (!getSlotIfNotNone(StepSlot, step))
        step = 1;
    if (step == 0)
        return;

    if (getSlotIfNotNone(StartSlot, start))
        start = ClampIndex(WrapIndex(start, length), length, true);
    else if (step > 0)
        start = 0;
    else
        start = length - 1;

    if (getSlotIfNotNone(StopSlot, stop))
        stop = ClampIndex(WrapIndex(stop, length), length, false);
    else if (step > 0)
        stop = length;
    else
        stop = length > 0 ? -1 : 0;
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
