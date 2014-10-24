#ifndef __GC_H__
#define __GC_H__

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <type_traits>

using namespace std;

// Simple mark and sweep garbage collector.

struct Cell;
struct Marker;

namespace gc {
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
    explicit Root(T ptr = GCTraits<T>::nullValue()) {
        *this = ptr;
        insert();
    }

    Root(const Root& other) {
        *this = other.ptr;
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
        assert(GCTraits<T>::isNonNull(ptr));
        GCTraits<T>::checkValid(ptr);
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

// A handled to a traced location
template <typename T>
struct Traced
{
    Traced(Root<T>& root) : handle_(&root) {}
    Traced(GlobalRoot<T>& root) : handle_(&root) {}

    operator T () { return *handle_; }
    operator const T () const { return *handle_; }
    T operator->() { return *handle_; }
    const T operator->() const { return *handle_; }
    T get() { return *handle_; }
    const T get() const { return *handle_; }

  private:
    T* handle_;
};

#endif
