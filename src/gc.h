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

#ifdef DEBUG
extern bool logGC;
#endif

struct Cell;
struct Marker;
struct RootBase;
struct StackBase;

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
    StackBase* stackList;
    bool isSweeping;
#ifdef DEBUG
    bool isAllocating;
    unsigned unsafeCount;
#endif
    size_t collectAt;

    friend struct Cell;
    friend struct RootBase;
    friend struct StackBase;
    friend struct AutoAssertNoGC;
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
#ifdef DEBUG
        if (cell)
            cell->checkValid();
#endif
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
    RootBase()
      : next_(nullptr), prev_(nullptr) {
#ifdef DEBUG
          useCount_ = 0;
#endif
    }

#ifdef DEBUG
    ~RootBase() {
        assert(useCount_ == 0);
    }
#endif

    virtual void trace(Tracer& t) = 0;
    virtual void clear() = 0;

    void insert() {
        assert(!next_ && !prev_);
        next_ = gc.rootList;
        prev_ = nullptr;
        if (next_)
            next_->prev_ = this;
        gc.rootList = this;
    }

    void remove() {
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

    RootBase* next() {
        return next_;
    }

    // addUse() and removeUse() are used to assert that the lifetimes of
    // pointers to the root don't outlive the root itself.

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

  private:
    RootBase* next_;
    RootBase* prev_;
#ifdef DEBUG
    unsigned useCount_;
#endif
};

struct StackBase
{
    StackBase() : next_(nullptr) {}

    virtual void trace(Tracer& t) = 0;
    virtual void clear() = 0;

    void insert() {
        assert(!next_);
        next_ = gc.stackList;
        gc.stackList = this;
    }

    void remove() {
        assert(gc.stackList == this);
        gc.stackList = next_;
#ifdef DEBUG
        next_ = nullptr;
#endif
    }

    StackBase* next() {
        return next_;
    }

  private:
    StackBase* next_;
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

    virtual void clear() override {
        ptr_ = GCTraits<T>::nullValue();
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

    virtual void clear() override {
        ptr_ = GCTraits<T>::nullValue();
    }

    virtual void trace(Tracer& t) {
        gc.trace(t, &ptr_);
    }

  private:
    T ptr_;
};

// Roots a cell as long as it is alive, for stack use.
template <typename T>
struct Stack : public WrapperMixins<Stack<T>, T>, protected StackBase
{
    Stack() {
        ptr_ = GCTraits<T>::nullValue();
        insert();
    }

    explicit Stack(T ptr) {
        *this = ptr;
        insert();
    }

    Stack(const Stack& other) {
        *this = other.ptr_;
        insert();
    }

    Stack(const GlobalRoot<T>& other) {
        *this = other.get();
        insert();
    }

    template <typename S>
    explicit Stack(const S& other) : ptr_(other) {
        insert();
    }

    ~Stack() { remove(); }

    define_comparisions;
    define_immutable_accessors;
    define_mutable_accessors;

    const T* location() const { return &ptr_; }

    template <typename S>
    Stack& operator=(const S& ptr) {
        ptr_ = ptr;
        GCTraits<T>::checkValid(ptr_);
        return *this;
    }

    Stack& operator=(const Stack& other) {
        *this = other.ptr_;
        return *this;
    }

    Stack& operator=(const GlobalRoot<T>& other) {
        *this = other.get();
        return *this;
    }

    virtual void clear() override {
        ptr_ = GCTraits<T>::nullValue();
    }

    virtual void trace(Tracer& t) {
        gc.trace(t, &ptr_);
    }

  private:
    T ptr_;
};

template <typename T>
struct MutableTraced;

// A handle to a traced location
template <typename T>
struct Traced : public WrapperMixins<Traced<T>, T>
{
    Traced(const Traced<T>& other) : ptr_(other.ptr_) {}
    Traced(Root<T>& root) : ptr_(root.get()) {}
    Traced(Stack<T>& root) : ptr_(root.get()) {}
    Traced(GlobalRoot<T>& root) : ptr_(root.get()) {}

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

    const T* location() const { return &ptr_; }

    static Traced<T> fromTracedLocation(T const* traced) {
        return Traced(traced);
    }

  private:
    Traced(const T* traced) : ptr_(*traced) {}

    const T& ptr_;
};

// A mutable handle to a traced location
template <typename T>
struct MutableTraced : public WrapperMixins<Traced<T>, T>
{
    MutableTraced(const MutableTraced<T>& other) : ptr_(other.ptr_) {}
    MutableTraced(Root<T>& root) : ptr_(root.get()) {}
    MutableTraced(Stack<T>& root) : ptr_(root.get()) {}
    MutableTraced(GlobalRoot<T>& root) : ptr_(root.get()) {}

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

    const T* location() const { return &ptr_; }

    static MutableTraced<T> fromTracedLocation(T* traced) {
        return MutableTraced(traced);
    }

  private:
    MutableTraced(const T* traced) : ptr_(*traced) {}

    T& ptr_;
};

template <typename T>
struct TracedVector;

template <typename T>
struct RootVector : private vector<T>, protected RootBase
{
    typedef vector<T> VectorBase;

    using VectorBase::back;
    using VectorBase::empty;
    using VectorBase::size;
    using VectorBase::begin;
    using VectorBase::end;
    using VectorBase::pop_back;
    using VectorBase::emplace_back;

    RootVector() {
        RootBase::insert();
    }

    RootVector(size_t initialSize)
    : vector<T>(initialSize) {
        RootBase::insert();
    }

    RootVector(const TracedVector<T>& v);

    ~RootVector() {
        RootBase::remove();
    }

    virtual void clear() override {
        VectorBase::resize(0);
    }

    Traced<T> ref(unsigned index) {
        assert(index < this->size());
        return Traced<T>::fromTracedLocation(&(*this)[index]);
    }

    void resize(size_t size) {
        VectorBase::resize(size);
    }

    void push_back(T element) {
        GCTraits<T>::checkValid(element);
        VectorBase::push_back(element);
    }

    T& operator[](size_t index) {
        assert(index < size());
        T& element = *(this->begin() + index);
        GCTraits<T>::checkValid(element);
        return element;
    }

    const T& operator[](size_t index) const {
        assert(index < size());
        const T& element = *(this->begin() + index);
        GCTraits<T>::checkValid(element);
        return element;
    }

    void trace(Tracer& t) override {
        for (auto i = this->begin(); i != this->end(); ++i)
            gc.trace(t, &*i);
    }

    template <typename S> friend struct TracedVector;
};

template <typename T>
struct TracedVector
{
    TracedVector(RootVector<T>& source)
      : root_(source), offset_(0), size_(source.size())
    {
        root_.addUse();
    }

    TracedVector(RootVector<T>& source, size_t offset, size_t size)
      : root_(source), offset_(offset), size_(size)
    {
        assert(offset + size <= source.size());
        root_.addUse();
    }

    TracedVector(const TracedVector<T>& source, size_t offset, size_t size)
      : root_(source.root_), offset_(source.offset_ + offset), size_(size)
    {
        assert(offset + size <= source.size());
        root_.addUse();
    }

    TracedVector(const TracedVector& other)
      : root_(other.root_), offset_(other.offset_), size_(other.size_)
    {
        root_.addUse();
    }

    ~TracedVector() {
        root_.removeUse();
    }

    size_t size() const {
        return size_;
    }

    Traced<T> operator[](unsigned index) const {
        assert(index < size_);
        return Traced<T>::fromTracedLocation(&root_[index + offset_]);
    }

    // todo: implement c++ iterators

  private:
    RootVector<T>& root_;
    size_t offset_;
    size_t size_;

    TracedVector& operator=(const TracedVector& other) = delete;
};

template <typename T>
RootVector<T>::RootVector(const TracedVector<T>& v)
  : vector<T>(v.size())
{
    RootBase::insert();
    for (size_t i = 0; i < v.size(); i++)
        (*this)[i] = v[i];
}

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

    virtual void clear() override {}

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

#undef define_immutable_accessors
#undef define_mutable_accessors

#endif
