#include "gc.h"

#include <algorithm>
#include <cstring>
#include <list>
#include <vector>
#include <iostream>

GC gc;

#ifdef DEBUG

bool logGC = false;

static inline void log(const char* s) {
    if (logGC)
        cout << "gc: " << s << endl;
}
static inline void log(const char* s, void *p) {
    if (logGC)
        cout << "gc: " << s << " " << p << endl;
}
static inline void log(const char* s, void *p, void *p2) {
    if (logGC)
        cout << "gc: " << s << " " << p << " " << p2 << endl;
}
static inline void log(const char* s, size_t i, int j) {
    if (logGC)
        cout << "gc: " << s << " " << dec << i << " " << j << endl;
}

#else

static inline void log(const char* s) {}
static inline void log(const char* s, void *p) {}
static inline void log(const char* s, void *p, void *p2) {}
static inline void log(const char* s, size_t i, int j) {}

#endif

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

#ifdef DEBUG
void Cell::checkValid() const
{
    if (epoch_ != gc.currentEpoch && epoch_ != gc.prevEpoch)
        cerr << "Bad cell at " << hex << this << endl;
    assert(epoch_ == gc.currentEpoch || epoch_ == gc.prevEpoch);
}
#endif

bool Cell::shouldMark()
{
#ifdef DEBUG
    checkValid();
#endif
    return epoch_ == gc.prevEpoch;
}

bool Cell::shouldSweep()
{
#ifdef DEBUG
    checkValid();
#endif
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

GC::GC()
  :
#ifdef DEBUG
    minCollectAt(10000),
#else
    minCollectAt(100),
#endif
    scheduleFactorPercent(200),
    currentEpoch(1),
    prevEpoch(2),
    cellCount(0),
    cells(sizeClassCount),
    freeCells(sizeClassCount),
    isSweeping(false),
#ifdef DEBUG
    isAllocating(false),
    unsafeCount(0),
#endif
    collectAt(minCollectAt)
{}

Cell* GC::allocCell(SizeClass sc)
{
    assert(sc < sizeClassCount);
    assert(sc < cells.size());
    assert(sc < freeCells.size());
    size_t allocSize = sizeFromClass(sc);
    assert(allocSize >= sizeof(Cell));

    Cell* cell;
    if (!freeCells[sc].empty()) {
        cell = freeCells[sc].back();
        freeCells[sc].pop_back();
    } else {
        cell = static_cast<Cell*>(malloc(allocSize));
    }

    // Pre-initialize memory to zero so that if we trigger GC by allocating in a
    // constructor then any pointers in the partially constructed object will be
    // in a valid state.
    memset(static_cast<void*>(cell), 0, allocSize);

    cells[sc].push_back(cell);
    cellCount++;
    return cell;
}

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

void GC::freeOldFreeCells(vector<Cell*>& cells)
{
    for_each(cells.begin(), cells.end(), []  (Cell* cell) {
        free(cell);
    });
    cells.clear();
}

void GC::destroyCells(vector<Cell*>& cells, vector<Cell*>::iterator dying,
                      vector<Cell*>& freeCells, size_t size)
{
    for (auto i = dying; i != cells.end(); i++) {
        Cell* cell = *i;
        Cell::destroyCell(cell, size);
        freeCells.push_back(cell);
    }
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
    for (StackBase* r = stackList; r; r = r->next())
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
        freeOldFreeCells(freeCells[sc]);
    for (SizeClass sc = 0; sc < sizeClassCount; sc++)
        destroyCells(cells[sc], dying[sc], freeCells[sc], sizeFromClass(sc));
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
    for (StackBase* r = stackList; r; r = r->next())
        r->clear();
    collect();
    assert(cellCount == 0);
}
