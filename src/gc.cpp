#include "gc.h"

#include <algorithm>
#include <cstring>
#include <list>
#include <vector>
#include <iostream>

//#define TRACE_GC

#ifdef TRACE_GC
static inline void log(const char* s) {
    cerr << s << endl;
}
static inline void log(const char* s, void *p) {
    cerr << s << " " << p << endl;
}
static inline void log(const char* s, void *p, void *p2) {
    cerr << s << " " << p << " " << p2 << endl;
}
static inline void log(const char* s, size_t i, int j) {
    cerr << s << " " << dec << i << " " << j << endl;
}
#else
static inline void log(const char* s) {}
static inline void log(const char* s, void *p) {}
static inline void log(const char* s, void *p, void *p2) {}
static inline void log(const char* s, size_t i, int j) {}
#endif

#ifdef BUILD_TESTS
GC gc(10, 110);
#else
GC gc(100, 200);
#endif

GC::GC(size_t minCollectAt, unsigned scheduleFactorPercent)
  : currentEpoch(1),
    prevEpoch(2),
    cellCount(0),
    cells(sizeClassCount),
    isSweeping(false),
#ifdef DEBUG
    isAllocating(false),
    unsafeCount(0),
#endif
    minCollectAt(minCollectAt),
    scheduleFactorPercent(scheduleFactorPercent),
    collectAt(minCollectAt)
{}

void GC::registerCell(Cell* cell, SizeClass sc)
{
    assert(!cells.empty());
    assert(sc < sizeClassCount);
    cells[sc].push_back(cell);
    cellCount++;
}

Cell::Cell()
{
    assert(gc.isAllocating);
    assert(!gc.isSweeping);
    assert((size_t(this) & ((1 << GC::sizeSmallAlignShift) - 1)) == 0);

    log("created", this);
    epoch_ = gc.currentEpoch;
#ifdef DEBUG
    gc.isAllocating = false;
#endif
}

Cell::~Cell()
{
    assert(gc.isSweeping);
}

void Cell::print(ostream& s) const {
    s << "Cell@" << hex << this;
}

void Cell::checkValid() const
{
    assert(epoch_ == gc.currentEpoch || epoch_ == gc.prevEpoch);
}

bool Cell::shouldMark()
{
    checkValid();
    return epoch_ == gc.prevEpoch;
}

bool Cell::shouldSweep()
{
    checkValid();
    return epoch_ == gc.prevEpoch;
}

bool Cell::isDying() const
{
    assert(gc.isSweeping);
    assert(epoch_ == gc.currentEpoch ||
           epoch_ == gc.prevEpoch);
    return epoch_ != gc.currentEpoch;
}

bool Cell::maybeMark(Cell** cellp)
{
    Cell* cell = *cellp;
    if (cell->shouldMark()) {
        cell->epoch_ = gc.currentEpoch;
        log("  marked", cell);
        return true;
    }

    return false;
}

void Cell::sweepCell(Cell* cell)
{
    log("  sweeping", cell);
    assert(cell->shouldSweep());
    cell->sweep();
}

void Cell::destroyCell(Cell* cell, size_t size)
{
    log("  destroy", cell);
    assert(cell->shouldSweep());
    cell->~Cell();
#ifdef DEBUG
    memset(reinterpret_cast<uint8_t*>(cell), 0xff, size);
#endif
    free(cell);
}

void RootBase::insert()
{
    log("insert root", this);
    assert(this);
    assert(!next_ && !prev_);

    next_ = gc.rootList;
    prev_ = nullptr;
    if (next_)
        next_->prev_ = this;
    gc.rootList = this;
}

void RootBase::remove()
{
    log("remove root", this);
    assert(this);
    assert(gc.rootList == this || prev_);

    if (next_)
        next_->prev_ = prev_;
    if (prev_)
        prev_->next_ = next_;
    else
        gc.rootList = next_;
    next_ = nullptr;
    prev_ = nullptr;
}

struct Marker : public Tracer
{
    virtual void visit(Cell** cellp) {
        if (*cellp && Cell::maybeMark(cellp)) {
            log("  pushed", cellp, *cellp);
            stack_.push_back(*cellp);
        }
    }

    void markRecursively() {
        while (!stack_.empty()) {
            Cell* cell = stack_.back();
            stack_.pop_back();
            cell->traceChildren(*this);
        }
    }

  private:
    // Stack of marked cells whose children have not yet been traced.
    vector<Cell*> stack_;
};

#ifdef DEBUG

#endif

void GC::maybeCollect()
{
    assert(unsafeCount == 0);
    if (cellCount >= collectAt)
        collect();
}

vector<Cell*>::iterator GC::sweepCells(vector<Cell*>& cells)
{
    auto dying = partition(cells.begin(), cells.end(), [] (Cell* cell) {
        return !cell->shouldSweep();
    });
    for_each(dying, cells.end(), Cell::sweepCell);
    return dying;
}

void GC::destroyCells(vector<Cell*>& cells, vector<Cell*>::iterator dying,
                      size_t size)
{
    for_each(dying, cells.end(), [=]  (Cell* cell) {
        Cell::destroyCell(cell, size);
    });
    size_t count = cells.end() - dying;
    assert(gc.cellCount >= count);
    gc.cellCount -= count;
    cells.erase(dying, cells.end());
}

void GC::collect()
{
    assert(!isSweeping);

    log("> GC::collect", cellCount, currentEpoch);

    // Begin new epoch
    prevEpoch = currentEpoch;
    currentEpoch++;
    if (currentEpoch == epochCount + 1)
        currentEpoch = 1;
    assert(currentEpoch >= 1 && currentEpoch <= epochCount);

    // Mark roots
    log("- marking roots");
    Marker marker;
    for (RootBase* r = rootList; r; r = r->next())
        r->trace(marker);

    // Mark
    log("- marking reachable");
    marker.markRecursively();

    // Sweep
    log("- sweeping");
    isSweeping = true;
    vector<vector<Cell*>::iterator> dying(sizeClassCount);
    for (SizeClass sc = 0; sc < sizeClassCount; sc++)
        dying[sc] = sweepCells(cells[sc]);
    for (SizeClass sc = 0; sc < sizeClassCount; sc++)
        destroyCells(cells[sc], dying[sc], sizeFromClass(sc));
    isSweeping = false;

    // Schedule next collection
    assert(scheduleFactorPercent > 100);
    double factor = static_cast<double>(scheduleFactorPercent) / 100;
    collectAt = max(minCollectAt, static_cast<size_t>(cellCount * factor));

    log("< GC::collect", cellCount, currentEpoch);
}

void GC::shutdown()
{
    for (RootBase* r = rootList; r; r = r->next())
        r->clear();
    collect();
    assert(cellCount == 0);
}

#ifdef BUILD_TESTS

#include "test.h"

struct TestCell : public Cell
{
    virtual void traceChildren(Tracer& t) {
        for (auto i = children_.begin(); i != children_.end(); ++i)
            gc.trace(t, &(*i));
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

    Root<TestCell*> r;
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

#endif
