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

    virtual void traceChildren(Tracer& t) = 0;
    virtual size_t size() const = 0;
    virtual void print(ostream& s) const = 0;
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

// todo: should be Cell&
inline ostream& operator<<(ostream& s, const Cell* cell) {
    cell->print(s);
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

#define define_smart_ptr_getters                                              \
    operator T () { return ptr_; }                                            \
    operator const T () const { return ptr_; }                                \
    T operator->() { return ptr_; }                                           \
    const T operator->() const { return ptr_; }                               \
    T get() { return ptr_; }                                                  \
    const T get() const { return ptr_; }                                      \
    T* operator&() { return &ptr_; }

// Roots a cell as long as it is alive.
template <typename T>
struct Root : protected RootBase
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

    ~Root() { remove(); }

    define_smart_ptr_getters;

    Root& operator=(T ptr) {
        GCTraits<T>::checkValid(ptr);
        ptr_ = ptr;
        return *this;
    }

    Root& operator=(const Root& other) {
        *this = other.ptr_;
        return *this;
    }

    virtual void trace(Tracer& t) {
        gc::trace(t, &ptr_);
    }

  private:
    T ptr_;
};

// Roots a cell.
//
// Intended for globals, it has deferred initialization and can cannot be reassigned.
template <typename T>
struct GlobalRoot : protected RootBase
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

    define_smart_ptr_getters;

    GlobalRoot& operator=(const GlobalRoot& other) = delete;

    virtual void trace(Tracer& t) {
        gc::trace(t, &ptr_);
    }

  private:
    T ptr_;
};

template <typename T>
struct TracedMixins {};

// A handle to a traced location
template <typename T>
struct Traced : public TracedMixins<T>
{
    Traced(Root<T>& root) : handle_(&root) {}
    Traced(GlobalRoot<T>& root) : handle_(&root) {}

    operator T () { return *handle_; }
    operator const T () const { return *handle_; }
    T operator->() { return *handle_; }
    const T operator->() const { return *handle_; }
    T get() { return *handle_; }
    const T get() const { return *handle_; }

    const T* location() const { return handle_; }

    static Traced<T> fromTracedLocation(T* traced) {
        return Traced(traced);
    }

  private:
    Traced(T* traced) : handle_(traced) {}

    T* handle_;
};

template <typename T>
struct std::hash<Traced<T>> {
    size_t operator()(Traced<T> t) const { return std::hash<T>()(t.get()); }
};

template <typename T>
struct RootVector : public vector<T>, private RootBase
{
    RootVector() {
#ifdef DEBUG
        useCount = 0;
#endif
        insert();
    }

    ~RootVector() {
        assert(useCount == 0);
        remove();
    }

    Traced<T> ref(unsigned index) {
        assert(index < this->size());
        return Traced<T>::fromTracedLocation(&(*this)[index]);
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

    define_smart_ptr_getters;

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
    AllocRoot<T*> t(static_cast<T*>(malloc(sizeof(T))));
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

#endif
