#ifndef __SLICE_H__
#define __SLICE_H__

#include "object.h"

struct Slice : public Object
{
    static void init();

    static GlobalRoot<Class*> ObjectClass;
    static GlobalRoot<Layout*> InitialLayout;

    Slice(TracedVector<Value> args);
    Slice(Traced<Class*> cls);

    void indices(int32_t length,
                 int32_t& startOut, int32_t& stopOut, int32_t& stepOut);

    bool getIterationData(int32_t length,
                          int32_t& startOut, int32_t& countOut,
                          int32_t& stepOut, MutableTraced<Value> resultOut);

  private:
    enum {
        StartSlot,
        StopSlot,
        StepSlot
    };

    int32_t getSlotOrDefault(unsigned slot, int32_t def);
};

inline int32_t WrapIndex(int32_t index, int32_t length)
{
    return min(index >= 0 ? index : length + index, length);
}

inline int32_t ClampIndex(int32_t index, int32_t length, bool inclusive)
{
    return max(min(index, length - (inclusive ? 1 : 0)), 0);
}

#endif
