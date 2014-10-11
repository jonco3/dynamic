#include "gc.h"

#include "test.h"

#include <list>
#include <vector>

//#define TRACE_GC

static const uint8_t InvalidEpoc = -1;

namespace gc {
static const int8_t epocs = 2;
static int8_t currentEpoc = 0;
static int8_t prevEpoc = 0;
static vector<Cell*> cells;
static list<Cell**> roots;
}

Cell::Cell() :
  epoc_(gc::currentEpoc)
{
    gc::cells.push_back(this);
}

Cell::~Cell()
{
    epoc_ = InvalidEpoc;
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
        delete cell;
#ifdef TRACE_GC
        cerr << "  swept " << hex << reinterpret_cast<uintptr_t>(cell) << endl;
#endif
        return true;
    }

    return false;
}

void RootBase::add(Cell** cellp)
{
    assert(cellp);
    gc::roots.push_back(cellp);
}

void RootBase::remove(Cell** cellp)
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
    cerr << "> gc::collect" << endl;
#endif

    // Begin new epoc
    prevEpoc = currentEpoc;
    currentEpoc = (currentEpoc + 1) % epocs;

    // Gather roots
    Marker marker;
    for (auto i = roots.begin(); i != roots.end(); ++i)
        marker.visit(*i);

    // Mark
    marker.markRecursively();

    // Sweep
    cells.erase(remove_if(cells.begin(), cells.end(), Cell::sweep), cells.end());

#ifdef TRACE_GC
    cerr << "< gc::collect" << endl;
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

    void addChild(TestCell* cell) {
        children_.push_back(cell);
    }

  private:
    vector<TestCell*> children_;
};

testcase(gc)
{
    using namespace gc;

    testEqual(cellCount(), 0);
    collect();
    testEqual(cellCount(), 0);

    Root<TestCell> r;
    collect();
    testEqual(cellCount(), 0);
    r = new TestCell;
    testEqual(cellCount(), 1);
    collect();
    testEqual(cellCount(), 1);
    r = nullptr;
    collect();
    testEqual(cellCount(), 0);

    r = new TestCell;
    r->addChild(new TestCell);
    testEqual(cellCount(), 2);
    collect();
    testEqual(cellCount(), 2);
    r = nullptr;
    collect();
    testEqual(cellCount(), 0);

    r = new TestCell;
    TestCell* b = new TestCell;
    r->addChild(b);
    b->addChild(r);
    testEqual(cellCount(), 2);
    r = nullptr;
    collect();
    testEqual(cellCount(), 0);
}

