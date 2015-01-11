#include "gc.h"

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
static const int8_t invalidEpoc = -1;
static int8_t currentEpoc = 1;
static int8_t prevEpoc = 2;
static vector<Cell*> cells;
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
}

Cell::Cell()
{
    assert(gc::isAllocating);
    assert(!gc::isSweeping);

    log("created", this);
    epoc_ = gc::currentEpoc;
    gc::cells.push_back(this);
}

Cell::~Cell()
{
    assert(gc::isSweeping);
    epoc_ = gc::invalidEpoc;
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
           epoc_ == gc::prevEpoc ||
           epoc_ == gc::invalidEpoc);
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

void Cell::destroyCell(Cell* cell)
{
    log("  destroy", cell);
    assert(cell->shouldSweep());
#ifdef DEBUG
    size_t cellSize = cell->size();
#endif
    cell->~Cell();
    free(cell);
#ifdef DEBUG
    memset(reinterpret_cast<uint8_t*>(cell) + sizeof(Cell),
           0xff,
           cellSize - sizeof(Cell));
#endif
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

bool gc::setAllocating(bool state)
{
    bool oldState = isAllocating;
    isAllocating = state;
    return oldState;
}

void gc::maybeCollect()
{
    if (cells.size() >= collectAt)
        collect();
}

void gc::collect() {
    assert(!isSweeping);

    log("> gc::collect", cellCount(), currentEpoc);

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
    auto dying = partition(cells.begin(), cells.end(), [] (Cell* cell) {
        return !cell->shouldSweep();
    });
    for_each(dying, cells.end(), Cell::sweepCell);
    for_each(dying, cells.end(), Cell::destroyCell);
    cells.erase(dying, cells.end());
    isSweeping = false;

    // Schedule next collection
    collectAt = max(minCollectAt,
        static_cast<size_t>(static_cast<double>(cellCount()) * scheduleFactor));

    log("< gc::collect", cellCount(), currentEpoc);
}

size_t gc::cellCount() {
    return cells.size();
}

#ifdef BUILD_TESTS

#include "test.h"

struct TestCell : public Cell
{
    virtual void traceChildren(Tracer& t) {
        for (auto i = children_.begin(); i != children_.end(); ++i)
            gc::trace(t, &(*i));
    }

    virtual size_t size() const { return sizeof(*this); }

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

    collect();
    size_t initCount = cellCount();

    testEqual(cellCount(), initCount);
    collect();
    testEqual(cellCount(), initCount);

    Root<TestCell*> r;
    collect();
    testEqual(cellCount(), initCount);
    r = gc::create<TestCell>();
    testEqual(cellCount(), initCount + 1);
    collect();
    testEqual(cellCount(), initCount + 1);
    r = nullptr;
    collect();
    testEqual(cellCount(), initCount);

    r = gc::create<TestCell>();
    r->addChild(gc::create<TestCell>());
    testEqual(cellCount(), initCount + 2);
    collect();
    testEqual(cellCount(), initCount + 2);
    r = nullptr;
    collect();
    testEqual(cellCount(), initCount);

    r = gc::create<TestCell>();
    TestCell* b = gc::create<TestCell>();
    r->addChild(b);
    b->addChild(r);
    testEqual(cellCount(), initCount + 2);
    r = nullptr;
    collect();
    testEqual(cellCount(), initCount);
}

#endif
