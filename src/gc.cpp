#include "gc.h"

#include <algorithm>
#include <cstring>
#include <list>
#include <vector>
#include <iostream>

GC gc;

bool logGCStats = false;
size_t gcZealPeriod = SIZE_MAX;

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

inline bool Cell::shouldMark()
{
#ifdef DEBUG
    checkValid();
#endif
    return epoch_ == gc.prevEpoch;
}

inline bool Cell::shouldSweep()
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

inline bool Cell::maybeMark(Cell** cellp)
{
    Cell* cell = *cellp;
    if (cell->shouldMark()) {
        cell->epoch_ = gc.currentEpoch;
        log("  marked", cell);
        return true;
    }

    return false;
}

inline void Cell::sweepCell(SweptCell* cell)
{
    log("  sweeping", cell);
    assert(cell->shouldSweep());
    cell->sweep();
}

inline void Cell::destructCell(Cell* cell)
{
    assert(cell->shouldSweep());
    cell->~Cell();
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
            log("  trace", cell);
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
    minCollectAt(100),
#else
    minCollectAt(100000),
#endif
    scheduleFactorPercent(200),
    currentEpoch(1),
    prevEpoch(2),
    gcCount(0),
    cellCount(0),
    isSweeping(false),
#ifdef DEBUG
    isAllocating(false),
    unsafeCount(0),
    allocCount(0),
#endif
    collectAt(minCollectAt)
{
    registerStackRoots(StackRootList<Cell*>::Instance);
}

void GC::registerStackRoots(StackRootListBase& roots)
{
    assert(find(stackRootLists.begin(), stackRootLists.end(), &roots) == stackRootLists.end());
    stackRootLists.push_back(&roots);
}

void GC::unregisterStackRoots(StackRootListBase& roots)
{
    auto i = find(stackRootLists.begin(), stackRootLists.end(), &roots);
    assert(i != stackRootLists.end());
    stackRootLists.erase(i);
}

Cell* GC::allocCell(SizeClass sc, bool requiresSweep)
{
    assert(sc < sizeClassCount);
    size_t allocSize = sizeFromClass(sc);
    assert(allocSize >= sizeof(Cell));

    void* cell;
    if (!freeCells[sc].empty()) {
        cell = freeCells[sc].back();
        freeCells[sc].pop_back();
    } else {
        cell = malloc(allocSize);
    }

    // Posion memory in debug builds.  Constructors have to be careful when
    // initializing members to ensure that a GC during construction doesn't see
    // an untracable state.
#ifdef DEBUG
    memset(reinterpret_cast<uint8_t*>(cell), 0x0f, allocSize);
#endif

    if (requiresSweep)
        sweptCells[sc].push_back(static_cast<SweptCell*>(cell));
    else
        cells[sc].push_back(static_cast<Cell*>(cell));
    cellCount++;
 #ifdef DEBUG
    allocCount++;
#endif
    return static_cast<Cell*>(cell);
}

template <typename T>
typename vector<T*>::iterator GC::partitionDyingCells(vector<T*>& cells)
{
    return partition(cells.begin(), cells.end(), [] (T* cell) {
        return !cell->shouldSweep();
    });
}

void GC::sweepCells(vector<SweptCell*>& cells,
                       vector<SweptCell*>::iterator dying)
{
    for_each(dying, cells.end(), Cell::sweepCell);
}

void GC::freeOldFreeCells(vector<Cell*>& cells)
{
    for_each(cells.begin(), cells.end(), [] (Cell* cell) { free(cell); });
    cells.clear();
}

template <typename T>
void GC::destroyCells(vector<T*>& cells, typename vector<T*>::iterator dying,
                      vector<Cell*>& freeCells, size_t size, bool destruct)
{
    size_t count = cells.end() - dying;
    freeCells.reserve(freeCells.size() + count);
    for (auto i = dying; i != cells.end(); i++) {
        Cell* cell = *i;
        log("  destroy", cell);
        if (destruct)
            Cell::destructCell(cell);
#ifdef DEBUG
        memset(reinterpret_cast<uint8_t*>(cell), 0xff, size);
#endif
        freeCells.push_back(cell);
    }
    assert(gc.cellCount >= count);
    gc.cellCount -= count;
    cells.erase(dying, cells.end());
}

void GC::collect()
{
    assert(!isSweeping);

    gcCount++;

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
    for (RootBase* r = rootList; r; r = r->nextRoot())
        r->trace(marker);
    for (auto list : stackRootLists)
        list->trace(marker);

    // Mark
    log("- marking reachable");
    marker.markRecursively();

    // Sweep
    log("- sweeping");
    isSweeping = true;
    vector<Cell*>::iterator dyingCells[sizeClassCount];
    vector<SweptCell*>::iterator dyingSweptCells[sizeClassCount];
    for (SizeClass sc = 0; sc < sizeClassCount; sc++) {
        dyingCells[sc] = partitionDyingCells(cells[sc]);
        dyingSweptCells[sc] = partitionDyingCells(sweptCells[sc]);
    }
    for (SizeClass sc = 0; sc < sizeClassCount; sc++)
        sweepCells(sweptCells[sc], dyingSweptCells[sc]);
    for (SizeClass sc = 0; sc < sizeClassCount; sc++)
        freeOldFreeCells(freeCells[sc]);
    for (SizeClass sc = 0; sc < sizeClassCount; sc++) {
        size_t size = sizeFromClass(sc);
        destroyCells(cells[sc], dyingCells[sc], freeCells[sc], size, false);
        destroyCells(sweptCells[sc], dyingSweptCells[sc], freeCells[sc], size,
                     true);
    }
    isSweeping = false;

    // Schedule next collection
    assert(scheduleFactorPercent > 100);
    double factor = static_cast<double>(scheduleFactorPercent) / 100;
    collectAt = max(minCollectAt, static_cast<size_t>(cellCount * factor));

    log("< GC::collect", cellCount, currentEpoch);

    logStats();
}

void GC::shutdown()
{
    for (RootBase* r = rootList; r; r = r->nextRoot())
        r->clear();
    for (auto list : stackRootLists)
        list->clear();
    collect();
    assert(cellCount == 0);
}

void GC::logStats()
{
    if (!logGCStats)
        return;

    unsigned rootCount = 0;
    for (RootBase* r = rootList; r; r = r->nextRoot())
        rootCount++;

    unsigned stackRootCount = 0;
    for (auto list : stackRootLists)
        stackRootCount += list->count();

    cout << dec;
    cout << "GC stats" << endl;
    cout << "  epoc:         " << size_t(currentEpoch) << endl;
    cout << "  gc count:     " << gcCount << endl;
    cout << "  cell count:   " << cellCount << endl;
    cout << "  next trigger: " << collectAt << endl;
    cout << "  system roots: " << rootCount << endl;
    cout << "  stack roots:  " << stackRootCount << endl;
    cout << "  allocated cells:" << endl;
    for (SizeClass sc = 0; sc < sizeClassCount; sc++) {
        if (!cells[sc].empty()) {
            cout << "    " << sizeFromClass(sc) << ": ";
            cout << cells[sc].size() << endl;
        }
    }
    cout << "  allocated swept cells:" << endl;
    for (SizeClass sc = 0; sc < sizeClassCount; sc++) {
        if (!sweptCells[sc].empty()) {
            cout << "    " << sizeFromClass(sc) << ": ";
            cout << sweptCells[sc].size() << endl;
        }
    }
    cout << "  free cells:" << endl;
    for (SizeClass sc = 0; sc < sizeClassCount; sc++) {
        if (!freeCells[sc].empty()) {
            cout << "    " << sizeFromClass(sc) << ": ";
            cout << freeCells[sc].size() << endl;
        }
    }
}
