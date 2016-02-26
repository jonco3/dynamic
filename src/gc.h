#ifndef __GC_H__
#define __GC_H__

#include "utils.h"
#include "vector.h"

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

extern bool logGCStats;

#ifdef DEBUG
extern bool logGC;
extern size_t gcZealPeriod;
#endif

struct Cell;
struct Marker;
struct RootBase;
struct StackBase;
struct SweptCell;
template <typename T> struct Heap;
template <typename T, typename V = Vector<T>> struct VectorBase;
template <typename T, typename V = Vector<T>> struct HeapVector;
template <typename T> struct MutableTraced;

// A visitor that visits edges in the object graph.
struct Tracer
{
    virtual void visit(Cell** cellp) = 0;
};

struct GC
{
    size_t minCollectAt;
    unsigned scheduleFactorPercent;

    GC();

    void shutdown();

    template <typename T, typename... Args>
    inline T* create(Args&&... args);
    template <typename T, typename... Args>
    inline T* createSized(size_t size, Args&&... args);

    void collect();

    template <typename T>
    inline void traceUnbarriered(Tracer& t, T* ptr);

    template <typename T, typename V>
    inline void traceVectorUnbarriered(Tracer& t, VectorBase<T, V>* ptr);

    template <typename T>
    inline void trace(Tracer& t, Heap<T>* ptr);

    template <typename T, typename V>
    inline void traceVector(Tracer& t, HeapVector<T, V>* ptrs);

#ifdef DEBUG
    bool currentlySweeping() const {
        return isSweeping;
    }

    bool isSupressed() const {
        return collectAt == SIZE_MAX;
    }
#endif

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

    Cell* allocCell(SizeClass cc, bool requiresSweep);

    bool isDying(const Cell* cell);
    void maybeCollect();

    template <typename T>
    typename vector<T*>::iterator partitionDyingCells(vector<T*>& cells);
    void sweepCells(vector<SweptCell*>& cells,
                    vector<SweptCell*>::iterator dying);
    void freeOldFreeCells(vector<Cell*>& cells);
    template <typename T>
    void destroyCells(vector<T*>& cells,
                      typename vector<T*>::iterator dying,
                      vector<Cell*>& freeCells, size_t size, bool destruct);

    void logStats();

    int8_t currentEpoch;
    int8_t prevEpoch;
    size_t gcCount;
    size_t cellCount;
    vector<Cell*> cells[sizeClassCount];
    vector<SweptCell*> sweptCells[sizeClassCount];
    vector<Cell*> freeCells[sizeClassCount];
    RootBase* rootList;
    StackBase* stackList;
    bool isSweeping;
#ifdef DEBUG
    bool isAllocating;
    unsigned unsafeCount;
    size_t allocCount;
#endif
    size_t collectAt;

    friend struct Cell;
    friend struct RootBase;
    friend struct StackBase;
    friend struct AutoAssertNoGC;
    friend struct AutoSupressGC;
    friend void testcase_body_gc();
};

extern GC gc;

// Base class for all garbage collected classes.
struct Cell
{
    Cell();
    virtual ~Cell();

#ifdef DEBUG
    void checkValid() const;
#endif

    virtual void traceChildren(Tracer& t) {}
    virtual void print(ostream& s) const;

  protected:
    bool isDying() const;

  private:
    int8_t epoch_;

    bool shouldMark();
    bool shouldSweep();

    static bool maybeMark(Cell** cellp);
    static void sweepCell(SweptCell* cell);
    static void destructCell(Cell* cell);

    friend struct GC;
    friend struct Marker;
};

struct SweptCell : public Cell
{
    virtual void sweep() = 0;
};

inline ostream& operator<<(ostream& s, const Cell& cell) {
    cell.print(s);
    return s;
}

template <typename T> struct GCTraits {};

static Cell const * const NullCellPtr = nullptr;

template <typename T>
struct GCTraits<T*>
{
    static T* nullValue() {
        static_assert(is_base_of<Cell, T>::value, "Type T must be derived from Cell");
        return nullptr;
    }

    static T const * const & nullValueRef() {
        static_assert(is_base_of<Cell, T>::value, "Type T must be derived from Cell");
        return reinterpret_cast<T const * const &>(NullCellPtr);
    }

    static bool isNonNull(const T* cell) {
        static_assert(is_base_of<Cell, T>::value, "Type T must be derived from Cell");
        return cell != nullptr;
    }

#ifdef DEBUG
    static void checkValid(const T* cell) {
        static_assert(is_base_of<Cell, T>::value, "Type T must be derived from Cell");
        if (cell)
            cell->checkValid();
    }
#endif

    static void trace(Tracer& t, T** cellp) {
        static_assert(is_base_of<Cell, T>::value, "Type T must be derived from Cell");
        t.visit(reinterpret_cast<Cell**>(cellp));
    }
};

#ifdef DEBUG
#define maybeCheckValid(T, C) GCTraits<T>::checkValid(C)
#else
#define maybeCheckValid(T, C)
#endif

template <typename T>
void GC::traceUnbarriered(Tracer& t, T* ptr) {
    GCTraits<T>::trace(t, ptr);
}

template <typename T, typename V>
void GC::traceVectorUnbarriered(Tracer& t, VectorBase<T, V>* ptrs) {
    for (auto i: *ptrs)
        GCTraits<T>::trace(t, &i);
}

template <typename T>
void GC::trace(Tracer& t, Heap<T>* ptr) {
    GCTraits<T>::trace(t, &ptr->get());
}

template <typename T, typename V>
 void GC::traceVector(Tracer& t, HeapVector<T, V>* ptrs) {
    for (auto i: *ptrs)
        GCTraits<T>::trace(t, &i);
}

template <typename T, typename V = Vector<T>>
struct TracedVector;

// Provides usage count in debug builds so we can assert references don't live
// longer than their referent.
struct UseCountBase
{
#ifdef DEBUG
    UseCountBase() : useCount_(0) {}

    ~UseCountBase() {
        assert(!hasUses());
    }
#endif

  protected:
#ifdef DEBUG
    bool hasUses() const {
        return useCount_ != 0;
    }
#endif

    void addUse() {
#ifdef DEBUG
        useCount_++;
#endif
    }

    void removeUse() {
#ifdef DEBUG
        assert(useCount_ > 0);
        useCount_--;
#endif
    }

    void checkUses() const {
        assert(!hasUses());
    }

    template <typename T, typename V>
    friend struct TracedVector;

  private:
#ifdef DEBUG
    unsigned useCount_;
#endif
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

// Base class for wrapper classes a single GC pointer.
template <typename T>
struct PointerBase : public UseCountBase
{
    PointerBase(T ptr = GCTraits<T>::nullValue()) : ptr_(ptr) {}

    template <typename S>
    PointerBase(const S& init) : ptr_(init) {}

    define_comparisions;
    define_immutable_accessors;
    define_mutable_accessors;

  protected:
    T ptr_;
};

// Base class for wrapper classes a vector of GC pointers.
template <typename T, typename V>
struct VectorBase : public UseCountBase, public V
{
    VectorBase() {}
    VectorBase(size_t count) : V(count, GCTraits<T>::nullValue()) {}
    VectorBase(size_t count, const T& fill) : V(count, fill) {}

    using Base = V;
    using Base::size;
    using Base::begin;
    using Base::end;
    using UseCountBase::checkUses;

    T& operator[](size_t index) {
        assert(index < size());
        T& element = *(begin() + index);
        maybeCheckValid(T, element);
        return element;
    }

    const T& operator[](size_t index) const {
        assert(index < size());
        const T& element = *(begin() + index);
        maybeCheckValid(T, element);
        return element;
    }

    MutableTraced<T> ref(unsigned index) {
        return MutableTraced<T>::fromTracedLocation(&(*this)[index]);
    }

    // todo: the following need an implementation that calls checkUses and
    // forwards to Base:
    //   opeator=, clear, emplace, emplace_back, resize, swap

    void push_back(T element) {
        maybeCheckValid(T, element);
        checkUses();
        Base::push_back(element);
    }

    template <class InputIterator>
    void assign(InputIterator first, InputIterator last) {
        checkUses();
        Base::assign(first, last);
    }

    void assign(size_t count, const T& fillValue) {
        checkUses();
        Base::assign(count, fillValue);
    }

    typename Base::iterator erase(typename Base::iterator position) {
        checkUses();
        return Base::erase(position);
    }

    typename Base::iterator erase(typename Base::iterator first,
                                  typename Base::iterator last) {
        checkUses();
        return Base::erase(first, last);
    }

    typename Base::iterator insert(typename Base::iterator pos, const T& val) {
        checkUses();
        return Base::insert(pos, val);
    }

    void insert(typename Base::iterator pos, size_t count, const T& value) {
        checkUses();
        Base::insert(pos, count, value);
    }

    template <typename I>
    typename Base::iterator insert(typename Base::iterator pos,
                                   I&& first, I&& last) {
        checkUses();
        return Base::insert(pos, std::forward<I>(first), std::forward<I>(last));
    }
};

struct RootBase
{
    RootBase() : next_(nullptr), prev_(nullptr) {
        assert(!next_ && !prev_);
        next_ = gc.rootList;
        prev_ = nullptr;
        if (next_)
            next_->prev_ = this;
        gc.rootList = this;
    }

    ~RootBase() {
        assert(gc.rootList == this || prev_);
        if (next_)
            next_->prev_ = prev_;
        if (prev_)
            prev_->next_ = next_;
        else
            gc.rootList = next_;
#ifdef DEBUG
        next_ = nullptr;
        prev_ = nullptr;
#endif
    }

    virtual void trace(Tracer& t) = 0;
    virtual void clear() = 0;

    RootBase* nextRoot() {
        return next_;
    }

  private:
    RootBase* next_;
    RootBase* prev_;
};

struct StackBase
{
    StackBase() : next_(nullptr) {
        assert(!next_);
        next_ = gc.stackList;
        gc.stackList = this;
    }

    ~StackBase() {
        assert(gc.stackList == this);
        gc.stackList = next_;
#ifdef DEBUG
        next_ = nullptr;
#endif
    }

    virtual void trace(Tracer& t) = 0;
    virtual void clear() = 0;

    StackBase* nextRoot() {
        return next_;
    }

  private:
    StackBase* next_;
};

template <typename W, typename T>
struct WrapperMixins {};

template <typename T>
struct Traced;

// Roots a cell.
//
// Intended for globals, it has deferred initialization and can cannot be
// reassigned.
template <typename T>
struct GlobalRoot
  : public WrapperMixins<GlobalRoot<T>, T>,
    public PointerBase<T>,
    protected RootBase
{
    GlobalRoot() {}
    GlobalRoot(const GlobalRoot& other) = delete;

    void init(T ptr) {
        maybeCheckValid(T, ptr);
        this->ptr_ = ptr;
    }

    virtual void clear() override {
        this->ptr_ = GCTraits<T>::nullValue();
    }

    GlobalRoot& operator=(const GlobalRoot& other) = delete;

    void trace(Tracer& t) override {
        gc.traceUnbarriered(t, &this->ptr_);
    }
};

// Roots a cell as long as it is alive.
template <typename T>
struct Root
  : public WrapperMixins<Root<T>, T>,
    public PointerBase<T>,
    protected RootBase
{
    Root() {}

    Root(const Root& other)
      : PointerBase<T>(other.ptr_)
    {}

    template <typename S>
    explicit Root(const S& other)
      : PointerBase<T>(other)
    {}

    Root& operator=(const Root& other) {
        maybeCheckValid(T, other.get());
        this->ptr_ = other.get();
        return *this;
    }

    template <typename S>
    Root& operator=(const S& ptr) {
        this->ptr_ = ptr;
        maybeCheckValid(T, this->ptr_);
        return *this;
    }

    virtual void clear() override {
        this->ptr_ = GCTraits<T>::nullValue();
    }

    void trace(Tracer& t) override {
        gc.traceUnbarriered(t, &this->ptr_);
    }
};

// Roots a cell as long as it is alive, for stack use.
template <typename T>
struct Stack
  : public WrapperMixins<Stack<T>, T>,
    public PointerBase<T>,
    protected StackBase
{
    Stack() {}

    Stack(const Stack& other)
      : PointerBase<T>(other.get())
    {}

    template <typename S>
    explicit Stack(const S& other)
      : PointerBase<T>(other)
    {}

    Stack& operator=(const Stack& other) {
        maybeCheckValid(T, other.get());
        this->ptr_ = other.get();
        return *this;
    }

    template <typename S>
    Stack& operator=(const S& ptr) {
        this->ptr_ = ptr;
        maybeCheckValid(T, this->ptr_);
        return *this;
    }

    virtual void clear() override {
        this->ptr_ = GCTraits<T>::nullValue();
    }

    void trace(Tracer& t) override {
        gc.traceUnbarriered(t, &this->ptr_);
    }
};

// Wraps a heap based pointer, which must be traced by its owning class'
// traceChildren() method.
template <typename T>
struct Heap
  : public WrapperMixins<Heap<T>, T>,
    public PointerBase<T>
{
    Heap()
      : PointerBase<T>()
    {
#ifndef DEBUG
        static_assert(sizeof(Heap<T>) == sizeof(T),
                      "Heap<T> should be the same size as T");
#endif
    }

    Heap(const Heap& other)
      : PointerBase<T>(other.get())
    {}

    template <typename S>
    explicit Heap(const S& other)
      : PointerBase<T>(other)
    {}

    Heap& operator=(const Heap& other) {
        maybeCheckValid(T, other.get());
        this->ptr_ = other.get();
        return *this;
    }

    template <typename S>
    Heap& operator=(const S& ptr) {
        this->ptr_ = ptr;
        maybeCheckValid(T, this->ptr_);
        return *this;
    }
};

// A handle to a traced location
template <typename T>
struct Traced : public WrapperMixins<Traced<T>, T>
{
    Traced(const Traced<T>& other) : ptr_(other.ptr_) {}
    Traced(GlobalRoot<T>& root) : ptr_(root.get()) {}
    Traced(Root<T>& root) : ptr_(root.get()) {}
    Traced(Stack<T>& root) : ptr_(root.get()) {}
    Traced(Heap<T>& ptr) : ptr_(ptr.get()) {}

    Traced(nullptr_t null)
      : ptr_(const_cast<const T&>(GCTraits<T>::nullValueRef())) {}
    // todo: T is |S*| for some S, but we need |const S*|, hence the cast.

    template <typename S>
    Traced(Root<S>& other)
        : ptr_(reinterpret_cast<const T&>(other.get()))
    {
        static_assert(is_base_of<typename remove_pointer<T>::type,
                                 typename remove_pointer<S>::type>::value,
                      "Invalid conversion");
    }

    template <typename S>
    Traced(Stack<S>& other)
        : ptr_(reinterpret_cast<const T&>(other.get()))
    {
        static_assert(is_base_of<typename remove_pointer<T>::type,
                                 typename remove_pointer<S>::type>::value,
                      "Invalid conversion");
    }

    template <typename S>
    Traced(MutableTraced<S>& other)
        : ptr_(reinterpret_cast<const T&>(other.get()))
    {
        static_assert(is_base_of<typename remove_pointer<T>::type,
                                 typename remove_pointer<S>::type>::value,
                      "Invalid conversion");
    }

    define_comparisions;
    define_immutable_accessors;

    static Traced<T> fromTracedLocation(T const* traced) {
        return Traced(traced);
    }

  private:
    Traced(const T* traced) : ptr_(*traced) {}

    const T& ptr_;
};

// A mutable handle to a traced location
template <typename T>
struct MutableTraced : public WrapperMixins<MutableTraced<T>, T>
{
    MutableTraced(const MutableTraced<T>& other) : ptr_(other.ptr_) {}
    MutableTraced(GlobalRoot<T>& root) : ptr_(root.get()) {}
    MutableTraced(Root<T>& root) : ptr_(root.get()) {}
    MutableTraced(Stack<T>& root) : ptr_(root.get()) {}
    MutableTraced(Heap<T>& ptr) : ptr_(ptr.get()) {}

    template <typename S>
    MutableTraced(Root<S>& other)
        : ptr_(reinterpret_cast<const T&>(other.get()))
    {
        static_assert(is_base_of<typename remove_pointer<T>::type,
                                 typename remove_pointer<S>::type>::value,
                      "Invalid conversion");
    }

    template <typename S>
    MutableTraced(Stack<S>& other)
        : ptr_(reinterpret_cast<const T&>(other.get()))
    {
        static_assert(is_base_of<typename remove_pointer<T>::type,
                                 typename remove_pointer<S>::type>::value,
                      "Invalid conversion");
    }

    define_comparisions;
    define_immutable_accessors;
    define_mutable_accessors;

    MutableTraced& operator=(const T& ptr) {
        ptr_ = ptr;
        return *this;
    }

    MutableTraced& operator=(const MutableTraced& other) {
        ptr_ = other.get();
        return *this;
    }

    MutableTraced& operator=(const Traced<T>& other) {
        ptr_ = other.get();
        return *this;
    }

    MutableTraced& operator=(const GlobalRoot<T> other) {
        ptr_ = other.get();
        return *this;
    }

    MutableTraced& operator=(const Root<T> other) {
        ptr_ = other.get();
        return *this;
    }

    MutableTraced& operator=(const Stack<T> other) {
        ptr_ = other.get();
        return *this;
    }

    MutableTraced& operator=(const Heap<T> other) {
        ptr_ = other.get();
        return *this;
    }

    static MutableTraced<T> fromTracedLocation(T* traced) {
        return MutableTraced(traced);
    }

  private:
    MutableTraced(T* traced) : ptr_(*traced) {}

    T& ptr_;
};

template <typename T, typename V = Vector<T>>
struct RootVector : public VectorBase<T, V>, protected RootBase
{
    using Base = VectorBase<T, V>;

    RootVector() {}
    RootVector(size_t count) : VectorBase<T, V>(count) {}
    RootVector(size_t count, const T& fill) : VectorBase<T, V>(count, fill) {}

    RootVector(const TracedVector<T, V>& v);

    virtual void clear() override {
        Base::resize(0);
    }

    void trace(Tracer& t) override {
        gc.traceVectorUnbarriered(t, this);
    }

    friend struct TracedVector<T, V>;
};

template <typename T, typename V>
struct HeapVector : public VectorBase<T, V>
{
    HeapVector() {}
    HeapVector(size_t count) : VectorBase<T, V>(count) {}
    HeapVector(size_t count, const T& fill) : VectorBase<T, V>(count, fill) {}

    HeapVector(const TracedVector<T, V>& other);
};

template <typename T, typename V>
struct TracedVector
{
    typedef VectorBase<T, V> Base;

    TracedVector(VectorBase<T, V>& source)
      : vector_(source), offset_(0), size_(source.size())
    {
        vector_.addUse();
    }

    TracedVector(VectorBase<T, V>& source, size_t offset, size_t size)
      : vector_(source), offset_(offset), size_(size)
    {
        assert(offset + size <= source.size());
        vector_.addUse();
    }

    TracedVector(const TracedVector& source, size_t offset, size_t size)
      : vector_(source.vector_), offset_(source.offset_ + offset), size_(size)
    {
        assert(offset + size <= source.size());
        vector_.addUse();
    }

    TracedVector(const TracedVector& other)
      : vector_(other.vector_), offset_(other.offset_), size_(other.size_)
    {
        vector_.addUse();
    }

    ~TracedVector() {
        vector_.removeUse();
    }

    size_t size() const {
        return size_;
    }

    Traced<T> operator[](unsigned index) const {
        assert(index < size_);
        return Traced<T>::fromTracedLocation(&vector_[index + offset_]);
    }

    typename Base::iterator begin() {
        return vector_.begin() + offset_;
    }

    typename Base::iterator end() {
        return begin() + size_;
    }

  private:
    VectorBase<T, V>& vector_;
    size_t offset_;
    size_t size_;

    TracedVector& operator=(const TracedVector& other) = delete;
};

template <typename T, typename V>
RootVector<T, V>::RootVector(const TracedVector<T, V>& v)
  : VectorBase<T, V>(v.size())
{
    // todo: use proper STLness for this
    for (size_t i = 0; i < v.size(); i++)
        (*this)[i] = v[i];
}

template <typename T, typename V>
HeapVector<T, V>::HeapVector(const TracedVector<T, V>& v)
  : VectorBase<T, V>(v.size())
{
    // todo: use proper STLness for this
    for (size_t i = 0; i < v.size(); i++)
        (*this)[i] = v[i];
}

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

inline void GC::maybeCollect()
{
    assert(unsafeCount == 0);

#ifdef DEBUG
    if (!isSupressed() && allocCount % gcZealPeriod == 0) {
        collect();
        return;
    }
#endif

    if (cellCount > collectAt)
        collect();
}

template <typename T, typename... Args>
T* GC::createSized(size_t size, Args&&... args) {
    static_assert(is_base_of<Cell, T>::value,
                  "Type T must be derived from Cell");
    assert(size >= sizeof(T));

    maybeCollect();

    bool requiresSweep = is_base_of<SweptCell, T>::value;
    SizeClass cc = sizeClass(size);
    void* data = allocCell(cc, requiresSweep);

    Stack<T*> t(static_cast<T*>(data));
    assert(static_cast<Cell*>(t.get()) == data);
    assert(!isAllocating);
#ifdef DEBUG
    isAllocating = true;
#endif
    new (t.get()) T(std::forward<Args>(args)...);
    assert(!isAllocating);
    return t;
}

template <typename T, typename... Args>
T* GC::create(Args&&... args) {
    static_assert(is_base_of<Cell, T>::value,
                  "Type T must be derived from Cell");

    maybeCollect();

    bool requiresSweep = is_base_of<SweptCell, T>::value;
    SizeClass cc = sizeClass(sizeof(T));
    void* data = allocCell(cc, requiresSweep);

    Stack<T*> t(static_cast<T*>(data));
    assert(static_cast<Cell*>(t.get()) == data);
    assert(!isAllocating);
#ifdef DEBUG
    isAllocating = true;
#endif
    new (t.get()) T(std::forward<Args>(args)...);
    assert(!isAllocating);
    return t;
}

/*
 * Assert if a collection is possible in the lifetime of the object.
 */
struct AutoAssertNoGC
{
    AutoAssertNoGC() {
#ifdef DEBUG
        gc.unsafeCount++;
#endif
    }

    ~AutoAssertNoGC() {
#ifdef DEBUG
        assert(gc.unsafeCount > 0);
        gc.unsafeCount--;
#endif
    }
};

/*
 * Disable GC temporarily.
 */
struct AutoSupressGC
{
    AutoSupressGC() : asar(gc.collectAt, SIZE_MAX) {}

  private:
    AutoSetAndRestoreValue<size_t> asar;
};

#undef define_immutable_accessors
#undef define_mutable_accessors

#endif
