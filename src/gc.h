#ifndef __GC_H__
#define __GC_H__

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <ostream>
#include <type_traits>
#include <utility>
#include <vector>

using namespace std;

// Simple mark and sweep garbage collector.

struct Cell;
struct Marker;
struct RootBase;

// A visitor that visits edges in the object graph.
struct Tracer
{
    virtual void visit(Cell** cellp) = 0;
};

struct GC
{
    GC(size_t minCollectAt, unsigned scheduleFactorPercent);

    template <typename T, typename... Args>
    inline T* create(Args&&... args);

    void collect();

    template <typename T>
    inline void trace(Tracer& t, T* ptr);

  private:
    static const int8_t epochCount = 2;

    // Cell size is divided into classes.  The size class is different depending
    // on whether the cell is large or small -- cells up to 64 bytes are
    // considered small, otherwise they are large.  The threshold is set by
    // sizeLargeThresholdShift.
    //
    // Small cell sizes are rounded up to the next multiple of 8 bytes (this is
    // set by sizeSmallAlignShift).  Large sizes are rounded up to the next
    // power of two.

    static const size_t sizeSmallAlignShift = 3;
    static const size_t sizeLargeThresholdShift = 6;
    static const size_t sizeClassCount = 39;
    static const size_t smallSizeClassCount =
        (1 << (sizeLargeThresholdShift - sizeSmallAlignShift)) - 1;

    typedef unsigned SizeClass;
    static inline SizeClass sizeClass(size_t size);
    static inline size_t sizeFromClass(SizeClass sc);

    void registerCell(Cell* cell, SizeClass sc);

    bool isDying(const Cell* cell);
    void maybeCollect();

    vector<Cell*>::iterator sweepCells(vector<Cell*>& cells);
    void destroyCells(vector<Cell*>& cells, vector<Cell*>::iterator dying,
                      size_t size);

    int8_t currentEpoch;
    int8_t prevEpoch;
    size_t cellCount;
    vector<vector<Cell*>> cells;
    RootBase* rootList;
    bool isSweeping;
#ifdef DEBUG
    bool isAllocating;
    unsigned unsafeCount;
#endif
    size_t minCollectAt;
    unsigned scheduleFactorPercent;
    size_t collectAt;

    friend struct Cell;
    friend struct RootBase;
    friend struct AutoAssertNoGC;
    friend void testcase_body_gc();
};

extern GC gc;

// Base class for all garbage collected classes.
struct Cell
{
    Cell();
    virtual ~Cell();

    void checkValid() const;

    virtual void traceChildren(Tracer& t) {}
    virtual void print(ostream& s) const {
        s << "Cell@" << hex << this;
    }
    virtual void sweep() {}

  protected:
    bool isDying() const;

  private:
    int8_t epoch_;

    bool shouldMark();
    bool shouldSweep();

    static bool maybeMark(Cell** cellp);
    static void sweepCell(Cell* cell);
    static void destroyCell(Cell* cell, size_t size);

    friend struct GC;
    friend struct Marker;
};

inline ostream& operator<<(ostream& s, const Cell& cell) {
    cell.print(s);
    return s;
}

template <typename T> struct GCTraits {};

template <typename T>
struct GCTraits<T*>
{
    static T* nullValue() {
        static_assert(is_base_of<Cell, T>::value, "Type T must be derived from Cell");
        return nullptr;
    }

    static bool isNonNull(const T* cell) {
        static_assert(is_base_of<Cell, T>::value, "Type T must be derived from Cell");
        return cell != nullptr;
    }

    static void checkValid(const T* cell) {
        static_assert(is_base_of<Cell, T>::value, "Type T must be derived from Cell");
        if (cell)
            cell->checkValid();
    }

    static void trace(Tracer& t, T** cellp) {
        static_assert(is_base_of<Cell, T>::value, "Type T must be derived from Cell");
        t.visit(reinterpret_cast<Cell**>(cellp));
    }
};

template <typename T>
void GC::trace(Tracer& t, T* ptr) {
    GCTraits<T>::trace(t, ptr);
}

struct RootBase
{
  RootBase() : next_(nullptr), prev_(nullptr) {}
    virtual ~RootBase() {}
    virtual void trace(Tracer& t) = 0;

    void insert();
    void remove();
    RootBase* next() { return next_; }

  private:
    RootBase* next_;
    RootBase* prev_;
};

#define define_comparisions                                                    \
    bool operator==(T other) { return ptr_ == other; }                         \
    bool operator!=(T other) { return ptr_ != other; }

#define define_immutable_accessors                                             \
    operator T () const { return ptr_; }                                       \
    T operator->() const { return ptr_; }                                      \
    const T& get() const { return ptr_; }                                      \

#define define_mutable_accessors                                               \
    operator T& () { return ptr_; }                                            \
    T& get() { return ptr_; }

template <typename W, typename T>
struct WrapperMixins {};

template <typename T>
struct Traced;

// Roots a cell.
//
// Intended for globals, it has deferred initialization and can cannot be
// reassigned.
template <typename T>
struct GlobalRoot : public WrapperMixins<GlobalRoot<T>, T>, protected RootBase
{
    GlobalRoot() : ptr_(GCTraits<T>::nullValue()) {}
    GlobalRoot(const GlobalRoot& other) = delete;

    ~GlobalRoot() {
        if (GCTraits<T>::isNonNull(ptr_))
            remove();
    }

    void init(T ptr) {
        GCTraits<T>::checkValid(ptr);
        if (GCTraits<T>::isNonNull(ptr))
            insert();
        ptr_ = ptr;
    }

    define_comparisions;
    define_immutable_accessors;

    const T* location() const { return &ptr_; }

    GlobalRoot& operator=(const GlobalRoot& other) = delete;

    virtual void trace(Tracer& t) {
        gc.trace(t, &ptr_);
    }

  private:
    T ptr_;
};

// Roots a cell as long as it is alive.
template <typename T>
struct Root : public WrapperMixins<Root<T>, T>, protected RootBase
{
    Root() {
        ptr_ = GCTraits<T>::nullValue();
        insert();
    }

    explicit Root(T ptr) {
        *this = ptr;
        insert();
    }

    Root(const Root& other) {
        *this = other.ptr_;
        insert();
    }

    Root(const GlobalRoot<T>& other) {
        *this = other.get();
        insert();
    }

    template <typename S>
    explicit Root(const S& other) : ptr_(other) {
        insert();
    }

    ~Root() { remove(); }

    define_comparisions;
    define_immutable_accessors;
    define_mutable_accessors;

    const T* location() const { return &ptr_; }

    template <typename S>
    Root& operator=(const S& ptr) {
        ptr_ = ptr;
        GCTraits<T>::checkValid(ptr_);
        return *this;
    }

    Root& operator=(const Root& other) {
        *this = other.ptr_;
        return *this;
    }

    Root& operator=(const GlobalRoot<T>& other) {
        *this = other.get();
        return *this;
    }

    virtual void trace(Tracer& t) {
        gc.trace(t, &ptr_);
    }

  private:
    T ptr_;
};

// A handle to a traced location
template <typename T>
struct Traced : public WrapperMixins<Traced<T>, T>
{
    Traced(const Traced<T>& other) : ptr_(other.ptr_) {}
    Traced(Root<T>& root) : ptr_(root.get()) {}
    Traced(GlobalRoot<T>& root) : ptr_(root.get()) {}

    template <typename S>
    Traced(Root<S>& other)
        : ptr_(reinterpret_cast<const T&>(other.get()))
    {
        static_assert(is_base_of<typename remove_pointer<T>::type,
                                 typename remove_pointer<S>::type>::value,
                      "Invalid conversion");
    }

    define_comparisions;
    define_immutable_accessors;

    const T* location() const { return &ptr_; }

    static Traced<T> fromTracedLocation(T const* traced) {
        return Traced(traced);
    }

  private:
    Traced(const T* traced) : ptr_(*traced) {}

    const T& ptr_;
};

template <typename T>
struct RootVector : private vector<T>, private RootBase
{
    typedef vector<T> VectorBase;

    using VectorBase::back;
    using VectorBase::empty;
    using VectorBase::operator[]; // todo: is this ok?
    using VectorBase::pop_back;
    using VectorBase::resize;
    using VectorBase::size;

    RootVector() {
#ifdef DEBUG
        useCount = 0;
#endif
        RootBase::insert();
    }

  RootVector(size_t initialSize)
    : vector<T>(initialSize) {
#ifdef DEBUG
        useCount = 0;
#endif
        RootBase::insert();
    }

    ~RootVector() {
        assert(useCount == 0);
        remove();
    }

    Traced<T> ref(unsigned index) {
        assert(index < this->size());
        return Traced<T>::fromTracedLocation(&(*this)[index]);
    }

    void push_back(T element) {
        VectorBase::push_back(element);
    }

    virtual void trace(Tracer& t) {
        for (auto i = this->begin(); i != this->end(); ++i)
            gc.trace(t, &*i);
    }

    void addUse() {
#ifdef DEBUG
        ++useCount;
#endif
    }

    void removeUse() {
#ifdef DEBUG
        assert(useCount > 0);
        --useCount;
#endif
    }

  private:
#ifdef DEBUG
    unsigned useCount;
#endif
};

template <typename T>
struct TracedVector
{
    TracedVector(RootVector<T>& source)
      : source_(source), offset_(0), size_(source.size()) {
        source_.addUse();
    }

    TracedVector(RootVector<T>& source, unsigned offset, unsigned size)
      : source_(source), offset_(offset), size_(size) {
        assert(offset_ + size_ <= source_.size());
        source_.addUse();
    }

    TracedVector(const TracedVector<T>& source, unsigned offset, unsigned size)
      : source_(source.source_), offset_(source.offset_ + offset), size_(size) {
        assert(offset_ + size_ <= source_.size());
        source_.addUse();
    }

    TracedVector(const TracedVector& other)
      : source_(other.source_), offset_(other.offset_), size_(other.size_) {
        source_.addUse();
      }

    ~TracedVector() {
        source_.removeUse();
    }

    size_t size() const {
        return size_;
    }

    Traced<T> operator[](unsigned index) const {
        assert(index < size_);
        return Traced<T>::fromTracedLocation(&source_[index + offset_]);
    }

    // todo: implement c++ iterators

  private:
    RootVector<T>& source_;
    unsigned offset_;
    unsigned size_;

    TracedVector& operator=(const TracedVector& other) = delete;
};

// Root used in allocation that doesn't check its (uninitialised) pointer.
template <typename T>
struct AllocRoot : protected RootBase
{
    explicit AllocRoot(T ptr) {
        ptr_ = ptr;
        insert();
    }

    ~AllocRoot() { remove(); }

    define_comparisions;
    define_immutable_accessors;

    virtual void trace(Tracer& t) {
        gc.trace(t, &ptr_);
    }

  private:
    T ptr_;
};

GC::SizeClass GC::sizeClass(size_t size)
{
    assert(sizeLargeThresholdShift >= sizeSmallAlignShift);
    assert(size > 0);
    if (size <= (1u << sizeLargeThresholdShift))
        return (size - 1) >> sizeSmallAlignShift;

    size_t exp = sizeLargeThresholdShift;
    while ((size_t(1) << exp) < size)
        exp++;
    return (exp - sizeLargeThresholdShift) + smallSizeClassCount;
}

size_t GC::sizeFromClass(SizeClass sc)
{
    if (sc <= smallSizeClassCount)
        return (sc + 1) << sizeSmallAlignShift;
    return 1u << (sc - smallSizeClassCount + sizeLargeThresholdShift);
}

template <typename T, typename... Args>
T* GC::create(Args&&... args) {
    static_assert(is_base_of<Cell, T>::value,
                  "Type T must be derived from Cell");

    maybeCollect();

    SizeClass sc = sizeClass(sizeof(T));
    size_t allocSize = sizeFromClass(sc);
    assert(allocSize >= sizeof(T));

    // Pre-initialize memory to zero so that if we trigger GC by allocating in a
    // constructor then any pointers in the partially constructed object will be
    // in a valid state.
    void* data = calloc(1, allocSize);

    AllocRoot<T*> t(static_cast<T*>(data));
    assert(static_cast<Cell*>(t.get()) == data);
    assert(!isAllocating);
#ifdef DEBUG
    isAllocating = true;
#endif
    new (t.get()) T(std::forward<Args>(args)...);
    assert(!isAllocating);
    registerCell(t, sc);
    return t;
}

/*
 * Assert if a collection is possible in the lifetime of the object.
 */
struct AutoAssertNoGC
{
#ifdef DEBUG
    AutoAssertNoGC() {
        gc.unsafeCount++;
    }

    ~AutoAssertNoGC() {
        assert(gc.unsafeCount > 0);
        gc.unsafeCount--;
    }
#endif
};

#undef define_immutable_accessors
#undef define_mutable_accessors

#endif
