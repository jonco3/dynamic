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

namespace gc {
extern void maybeCollect();
extern void collect();
extern size_t cellCount();
extern bool isDying(const Cell* cell);
extern unsigned unsafeCount;
} // namespace gc

// A visitor that visits edges in the object graph.
struct Tracer
{
    virtual void visit(Cell** cellp) = 0;
};

// Base class for all garbage collected classes.
struct Cell
{
    Cell();
    virtual ~Cell();

    void checkValid() const;

    virtual void traceChildren(Tracer& t) {}
    virtual size_t size() const = 0;
    virtual void print(ostream& s) const {
        s << "Cell@" << hex << this;
    }
    virtual void sweep() {}

  protected:
    bool isDying() const;

  private:
    int8_t epoc_;

    bool shouldMark();
    bool shouldSweep();

    static bool maybeMark(Cell** cellp);
    static void sweepCell(Cell* cell);
    static void destroyCell(Cell* cell);

    friend struct Marker;
    friend void gc::collect();
};

inline ostream& operator<<(ostream& s, const Cell& cell) {
    cell.print(s);
    return s;
}

template <typename T> struct GCTraits {};

/*
template <>
struct GCTraits<nullptr_t>
{
    static nullptr_t nullValue() { return nullptr; }
    static bool isNonNull( nullptr_t cell) { return false; }
    static void checkValid(nullptr_t cell) {}
    static void trace(Tracer& t, nullptr_t** cellp) {}
};
*/

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

namespace gc {

template <typename T>
inline void trace(Tracer& t, T* ptr) {
    GCTraits<T>::trace(t, ptr);
}

} // namespace gc

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
        gc::trace(t, &ptr_);
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
        gc::trace(t, &ptr_);
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

namespace std {
template <typename T>
struct hash<Traced<T>> {
    size_t operator()(Traced<T> t) const { return std::hash<T>()(t.get()); }
};
} /* namespace std */

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
            gc::trace(t, &*i);
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

namespace gc {

extern bool setAllocating(bool state);

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
        gc::trace(t, &ptr_);
    }

  private:
    T ptr_;
};

template <typename T, typename... Args>
T* create(Args&&... args) {
    static_assert(is_base_of<Cell, T>::value, "Type T must be derived from Cell");
    maybeCollect();
    // Pre-initialize memory to zero so that if we trigger GC by allocating in a
    // constructor then any pointers in the partially constructed object will be
    // in a valid state.
    AllocRoot<T*> t(static_cast<T*>(calloc(1, sizeof(T))));
#ifdef DEBUG
    bool oldAllocState = setAllocating(true);
#endif
    new (t.get()) T(std::forward<Args>(args)...);
#ifdef DEBUG
    setAllocating(oldAllocState);
#endif
    return t;
}

} // namespace gc

/*
 * Assert if a collection is possible in the lifetime of the object.
 */
struct AutoAssertNoGC
{
    AutoAssertNoGC() { gc::unsafeCount++; }
    ~AutoAssertNoGC() {
        assert(gc::unsafeCount > 0);
        gc::unsafeCount--;
    }
};

#undef define_immutable_accessors
#undef define_mutable_accessors

#endif
