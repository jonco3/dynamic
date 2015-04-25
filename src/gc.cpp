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

namespace gc {
static const int8_t epocs = 2;
static int8_t currentEpoc = 1;
static int8_t prevEpoc = 2;
static size_t cellCount = 0;
static vector<vector<Cell*>> cells;
static RootBase* rootList;
static bool isSweeping = false;
static bool isAllocating = false;
#ifdef BUILD_TESTS
static size_t minCollectAt = 10;
static double scheduleFactor = 1.1;
#else
static size_t minCollectAt = 100;
static double scheduleFactor = 2;
#endif
static size_t collectAt = minCollectAt;

unsigned unsafeCount = 0;
}

void gc::init()
{
    assert(cellCount == 0);
    assert(cells.empty());
    cells.resize(sizeClassCount);
}

Cell::Cell()
{
    assert(gc::isAllocating);
    assert(!gc::isSweeping);

    log("created", this);
    epoc_ = gc::currentEpoc;
    gc::isAllocating = false;
}

void gc::registerCell(Cell* cell, SizeClass sc)
{
    assert(!cells.empty());
    assert(sc < sizeClassCount);
    cells[sc].push_back(cell);
    cellCount++;
}

Cell::~Cell()
{
    assert(gc::isSweeping);
}

void Cell::checkValid() const
{
    assert(epoc_ == gc::currentEpoc || epoc_ == gc::prevEpoc);
}

bool Cell::shouldMark()
{
    checkValid();
    return epoc_ == gc::prevEpoc;
}

bool Cell::shouldSweep()
{
    checkValid();
    return epoc_ == gc::prevEpoc;
}

bool Cell::isDying() const
{
    assert(gc::isSweeping);
    assert(epoc_ == gc::currentEpoc ||
           epoc_ == gc::prevEpoc);
    return epoc_ != gc::currentEpoc;
}

bool Cell::maybeMark(Cell** cellp)
{
    Cell* cell = *cellp;
    if (cell->shouldMark()) {
        cell->epoc_ = gc::currentEpoc;
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

    next_ = gc::rootList;
    prev_ = nullptr;
    if (next_)
        next_->prev_ = this;
    gc::rootList = this;
}

void RootBase::remove()
{
    log("remove root", this);
    assert(this);
    assert(gc::rootList == this || prev_);

    if (next_)
        next_->prev_ = prev_;
    if (prev_)
        prev_->next_ = next_;
    else
        gc::rootList = next_;
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

bool gc::getAllocating()
{
    return isAllocating;
}

void gc::setAllocating()
{
    assert(!isAllocating);
    isAllocating = true;
}

#endif

void gc::maybeCollect()
{
    assert(unsafeCount == 0);
    if (cells.size() >= collectAt)
        collect();
}

static vector<Cell*>::iterator sweepCells(vector<Cell*>& cells)
{
    auto dying = partition(cells.begin(), cells.end(), [] (Cell* cell) {
        return !cell->shouldSweep();
    });
    for_each(dying, cells.end(), Cell::sweepCell);
    return dying;
}

static void destroyCells(vector<Cell*>& cells, vector<Cell*>::iterator dying,
                         size_t size)
{
    for_each(dying, cells.end(), [=]  (Cell* cell) {
        Cell::destroyCell(cell, size);
    });
    size_t count = cells.end() - dying;
    assert(gc::cellCount >= count);
    gc::cellCount -= count;
    cells.erase(dying, cells.end());
}

void gc::collect() {
    assert(!isSweeping);

    log("> gc::collect", cellCount, currentEpoc);

    // Begin new epoc
    prevEpoc = currentEpoc;
    currentEpoc++;
    if (currentEpoc == epocs + 1)
        currentEpoc = 1;
    assert(currentEpoc >= 1 && currentEpoc <= epocs);

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
    collectAt = max(minCollectAt,
        static_cast<size_t>(static_cast<double>(cellCount) * scheduleFactor));

    log("< gc::collect", cellCount, currentEpoc);
}

#ifdef BUILD_TESTS

#include "test.h"

struct TestCell : public Cell
{
    virtual void traceChildren(Tracer& t) {
        for (auto i = children_.begin(); i != children_.end(); ++i)
            gc::trace(t, &(*i));
    }

    virtual void print(ostream& s) const {
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
    using namespace gc;

    testEqual(sizeClass(1), 0u);
    testEqual(sizeClass(1 << sizeSmallAlignShift), 0u);
    testEqual(sizeFromClass(0), 1u << sizeSmallAlignShift);
    testEqual(sizeClass(1 << sizeLargeThresholdShift),
              (1u << (sizeLargeThresholdShift - sizeSmallAlignShift)) - 1);
    testEqual(sizeClass((1 << sizeLargeThresholdShift) + 1),
              1u << (sizeLargeThresholdShift - sizeSmallAlignShift));
    for (size_t i = 1; i < 1024; i++) {
        SizeClass sc = sizeClass(i);
        testTrue(sizeFromClass(sc) >= i);
    }
    for (size_t i = sizeSmallAlignShift; i < 32; i++) {
        SizeClass sc = sizeClass(1u << i);
        testEqual(sizeFromClass(sc), 1u << i);
    }

    collect();
    size_t initCount = gc::cellCount;

    testEqual(gc::cellCount, initCount);
    collect();
    testEqual(gc::cellCount, initCount);

    Root<TestCell*> r;
    collect();
    testEqual(gc::cellCount, initCount);
    r = gc::create<TestCell>();
    testEqual(gc::cellCount, initCount + 1);
    collect();
    testEqual(gc::cellCount, initCount + 1);
    r = nullptr;
    collect();
    testEqual(gc::cellCount, initCount);

    r = gc::create<TestCell>();
    r->addChild(gc::create<TestCell>());
    testEqual(gc::cellCount, initCount + 2);
    collect();
    testEqual(gc::cellCount, initCount + 2);
    r = nullptr;
    collect();
    testEqual(gc::cellCount, initCount);

    r = gc::create<TestCell>();
    TestCell* b = gc::create<TestCell>();
    r->addChild(b);
    b->addChild(r);
    testEqual(gc::cellCount, initCount + 2);
    r = nullptr;
    collect();
    testEqual(gc::cellCount, initCount);
}

#endif
