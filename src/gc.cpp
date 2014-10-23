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
static inline void log(const char* s, size_t i) {
    cerr << s << " " << dec << i << endl;
}
#else
static inline void log(const char* s) {}
static inline void log(const char* s, void *p) {}
static inline void log(const char* s, void *p, void *p2) {}
static inline void log(const char* s, size_t i) {}
#endif

namespace gc {
static const int8_t epocs = 2;
static const int8_t invalidEpoc = -1;
static int8_t currentEpoc = 0;
static int8_t prevEpoc = 0;
static vector<Cell*> cells;
static RootBase* rootList;
static bool isSweeping = false;
static size_t collectAt = 10;
static double scheduleFactor = 1.5;
}

Cell::Cell()
{
    assert(!gc::isSweeping);

    if (gc::cells.size() >= gc::collectAt) {
        gc::collect();
    }

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

bool Cell::mark(Cell** cellp)
{
    Cell* cell = *cellp;
    if (cell->shouldMark()) {
        cell->epoc_ = gc::currentEpoc;
        log("  marked", cell);
        return true;
    }

    return false;
}

bool Cell::sweep(Cell* cell)
{
    if (cell->shouldSweep()) {
        log("  sweeping", cell);
#ifdef DEBUG
        size_t cellSize = cell->size();
#endif
        delete cell;
#ifdef DEBUG
        memset(reinterpret_cast<uint8_t*>(cell) + sizeof(Cell),
               0xff,
               cellSize - sizeof(Cell));
#endif
        return true;
    }

    return false;
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
        if (*cellp && Cell::mark(cellp)) {
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

void gc::collect() {
    log("> gc::collect", cellCount());

    // Begin new epoc
    prevEpoc = currentEpoc;
    currentEpoc = (currentEpoc + 1) % epocs;

    // Mark roots
    log("- marking roots");
    Marker marker;
    for (RootBase* r = rootList; r; r = r->next())
        r->trace(marker);

    // Mark
    log("- marking reachable");
    marker.markRecursively();

    // Sweep
    isSweeping = true;
    cells.erase(remove_if(cells.begin(), cells.end(), Cell::sweep), cells.end());
    isSweeping = false;

    // Schedule next collection
    collectAt = static_cast<size_t>(static_cast<double>(cellCount()) * scheduleFactor);

    log("< gc::collect", cellCount());
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
    r = new TestCell;
    testEqual(cellCount(), initCount + 1);
    collect();
    testEqual(cellCount(), initCount + 1);
    r = nullptr;
    collect();
    testEqual(cellCount(), initCount);

    r = new TestCell;
    r->addChild(new TestCell);
    testEqual(cellCount(), initCount + 2);
    collect();
    testEqual(cellCount(), initCount + 2);
    r = nullptr;
    collect();
    testEqual(cellCount(), initCount);

    r = new TestCell;
    TestCell* b = new TestCell;
    r->addChild(b);
    b->addChild(r);
    testEqual(cellCount(), initCount + 2);
    r = nullptr;
    collect();
    testEqual(cellCount(), initCount);
}

#endif
