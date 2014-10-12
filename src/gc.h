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

    // Visit a |DerivedCell* const *| that doesn't implicity convert to |Cell**|
    template <typename T>
    void visit(const T*const* cellp) {
        static_assert(is_base_of<Cell, T>::value, "Type T must be derived from Cell");
        visit(reinterpret_cast<Cell**>(const_cast<T**>(cellp)));
    }
};

// Base class for all garbage collected classes.
struct Cell
{
    Cell();
    virtual ~Cell();

    void checkValid();
    virtual void trace(Tracer& t) const = 0;
    virtual size_t size() const = 0;
    virtual void print(ostream& s) const = 0;

  protected:
    bool isDying() const;

  private:
    int8_t epoc_;

    bool shouldMark();
    bool shouldSweep();

    static bool mark(Cell** cellp);
    static bool sweep(Cell* cell);

    friend struct Marker;
    friend void gc::collect();
};

// todo: should be Cell&
inline ostream& operator<<(ostream& s, const Cell* cell) {
    cell->print(s);
    return s;
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

// Roots a cell as long as it is alive.
template <typename T>
struct Root : protected RootBase
{
    explicit Root(T* ptr = nullptr) : ptr_(ptr) {
        if (ptr_)
            ptr_->checkValid();
        insert();
    }

    Root(const Root& other) : ptr_(other.ptr_) {
        if (ptr_)
            ptr_->checkValid();
        insert();
    }

    ~Root() { remove(); }

    operator T* () { return ptr_; }
    operator const T* () const { return ptr_; }
    T* operator->() { return ptr_; }
    const T* operator->() const { return ptr_; }
    T* get() { return ptr_; }
    const T* get() const { return ptr_; }

    Root& operator=(T* ptr) {
        if (ptr)
            ptr->checkValid();
        ptr_ = ptr;
        return *this;
    }

    Root& operator=(const Root& other) {
        if (other.ptr_)
            other.ptr_->checkValid();
        *this = other.ptr_;
    }

    virtual void trace(Tracer& t) { t.visit(&ptr_); }

  private:
    T* ptr_;
};

// Roots a cell.
//
// Intended for globals, it has deferred initialization and can cannot be reassigned.
template <typename T>
struct GlobalRoot : protected RootBase
{
    GlobalRoot() : ptr_(nullptr) {}
    GlobalRoot(const GlobalRoot& other) = delete;

    ~GlobalRoot() {
        if (ptr_)
            remove();
    }

    void init(T* ptr) {
        assert(!ptr_);
        ptr->checkValid();
        insert();
        ptr_ = ptr;
    }

    operator T* () { return ptr_; }
    operator const T* () const { return ptr_; }
    T* operator->() { return ptr_; }
    const T* operator->() const { return ptr_; }
    T* get() { return ptr_; }
    const T* get() const { return ptr_; }

    GlobalRoot& operator=(const GlobalRoot& other) = delete;

    virtual void trace(Tracer& t) { t.visit(&ptr_); }

  private:
    T* ptr_;
};


#endif
