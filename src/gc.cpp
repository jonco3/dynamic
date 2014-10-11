#include "gc.h"

#include "test.h"

#include <list>
#include <vector>

//#define TRACE_GC

namespace gc {
static const int8_t epocs = 2;
static const int8_t invalidEpoc = -1;
static int8_t currentEpoc = 0;
static int8_t prevEpoc = 0;
static vector<Cell*> cells;
static list<Cell**> roots;
static bool isSweeping = false;
static size_t collectAt = 10;
static double scheduleFactor = 1.5;
}

Cell::Cell()
{
    if (gc::cells.size() >= gc::collectAt) {
        gc::collect();
    }

#ifdef TRACE_GC
    cerr << "created " << hex << reinterpret_cast<uintptr_t>(this) << endl;
#endif
    epoc_ = gc::currentEpoc;
    gc::cells.push_back(this);
}

Cell::~Cell()
{
    assert(gc::isSweeping);
    epoc_ = gc::invalidEpoc;
}

bool Cell::shouldMark()
{
    assert(epoc_ == gc::currentEpoc || epoc_ == gc::prevEpoc);
    return epoc_ == gc::prevEpoc;
}

bool Cell::shouldSweep()
{
    assert(epoc_ == gc::currentEpoc || epoc_ == gc::prevEpoc);
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
#ifdef TRACE_GC
        cerr << "  marked " << hex << reinterpret_cast<uintptr_t>(cell) << endl;
#endif
        return true;
    }

    return false;
}

bool Cell::sweep(Cell* cell)
{
    if (cell->shouldSweep()) {
#ifdef TRACE_GC
        cerr << "  sweeping " << hex << reinterpret_cast<uintptr_t>(cell) << endl;
#endif
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

void gc::addRoot(Cell** cellp)
{
    assert(cellp);
    gc::roots.push_back(cellp);
}

void gc::removeRoot(Cell** cellp)
{
    assert(cellp);
    // todo: use embedded list pointers to avoid this lookup
    gc::roots.remove(cellp);
}

struct Marker : public Tracer
{
    virtual void visit(Cell** cellp) {
        if (*cellp && Cell::mark(cellp))
            stack_.push_back(cellp);
    }

    void markRecursively() {
        while (!stack_.empty()) {
            Cell* cell = *stack_.back();
            stack_.pop_back();
            cell->trace(*this);
        }
    }

  private:
    // Stack of marked cells whose children have not yet been traced.
    vector<Cell**> stack_;
};

void gc::collect() {
#ifdef TRACE_GC
    cerr << "> gc::collect " << dec << cellCount() << endl;
#endif

    // Begin new epoc
    prevEpoc = currentEpoc;
    currentEpoc = (currentEpoc + 1) % epocs;

    // Gather roots
#ifdef TRACE_GC
    cerr << "- marking roots" << endl;
#endif
    Marker marker;
    for (auto i = roots.begin(); i != roots.end(); ++i) {
        marker.visit(*i);
    }

    // Mark
#ifdef TRACE_GC
    cerr << "- marking reachable" << endl;
#endif
    marker.markRecursively();

    // Sweep
    isSweeping = true;
    cells.erase(remove_if(cells.begin(), cells.end(), Cell::sweep), cells.end());
    isSweeping = false;

    // Schedule next collection
    collectAt = static_cast<size_t>(static_cast<double>(cellCount()) * scheduleFactor);

#ifdef TRACE_GC
    cerr << "< gc::collect " << dec << cellCount() << endl;
#endif
}

size_t gc::cellCount() {
    return cells.size();
}

struct TestCell : private Cell
{
    virtual void trace(Tracer& t) const {
        for (auto i = children_.begin(); i != children_.end(); ++i)
            t.visit(&(*i));
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

    Root<TestCell> r;
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

