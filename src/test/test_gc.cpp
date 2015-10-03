#include "src/gc.h"

#include "src/test.h"

#include <cstring>

struct TestCell : public Cell
{
    void traceChildren(Tracer& t) override {
        gc.traceVector(t, &children_);
    }

    void print(ostream& s) const override {
        s << "TestCell@" << hex << static_cast<const void*>(this);
    }

    void addChild(TestCell* cell) {
        children_.push_back(cell);
    }

  private:
    vector<TestCell*> children_;
};

testcase(gc)
{
    testEqual(GC::sizeClass(1), 0u);
    testEqual(GC::sizeClass(1 << GC::sizeSmallAlignShift), 0u);
    testEqual(GC::sizeFromClass(0), 1u << GC::sizeSmallAlignShift);
    testEqual(GC::sizeClass(1 << GC::sizeLargeThresholdShift),
              (1u << (GC::sizeLargeThresholdShift - GC::sizeSmallAlignShift)) - 1);
    testEqual(GC::sizeClass((1 << GC::sizeLargeThresholdShift) + 1),
              1u << (GC::sizeLargeThresholdShift - GC::sizeSmallAlignShift));
    for (size_t i = 1; i < 1024; i++) {
        GC::SizeClass sc = GC::sizeClass(i);
        testTrue(GC::sizeFromClass(sc) >= i);
    }
    for (size_t i = GC::sizeSmallAlignShift; i < 32; i++) {
        GC::SizeClass sc = GC::sizeClass(1u << i);
        testEqual(GC::sizeFromClass(sc), 1u << i);
    }

    gc.collect();
    size_t initCount = gc.cellCount;

    testEqual(gc.cellCount, initCount);
    gc.collect();
    testEqual(gc.cellCount, initCount);

    Stack<TestCell*> r;
    gc.collect();
    testEqual(gc.cellCount, initCount);
    r = gc.create<TestCell>();
    testEqual(gc.cellCount, initCount + 1);
    gc.collect();
    testEqual(gc.cellCount, initCount + 1);
    r = nullptr;
    gc.collect();
    testEqual(gc.cellCount, initCount);

    r = gc.create<TestCell>();
    r->addChild(gc.create<TestCell>());
    testEqual(gc.cellCount, initCount + 2);
    gc.collect();
    testEqual(gc.cellCount, initCount + 2);
    r = nullptr;
    gc.collect();
    testEqual(gc.cellCount, initCount);

    r = gc.create<TestCell>();
    TestCell* b = gc.create<TestCell>();
    r->addChild(b);
    b->addChild(r);
    testEqual(gc.cellCount, initCount + 2);
    r = nullptr;
    gc.collect();
    testEqual(gc.cellCount, initCount);
}
