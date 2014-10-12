#ifndef __GC_H__
#define __GC_H__

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <type_traits>

#include <iostream>

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
//
// The root is only added to the root list when it contains a non-null pointer
// to allow global Root<> instances to be added only after the GC has been
// initialised.
template <typename T>
struct Root : protected RootBase
{
    Root() : ptr_(nullptr) {}

    explicit Root(T* ptr) : ptr_(ptr) {
        if (ptr_) {
            ptr_->checkValid();
            insert();
        }
    }

    Root(const Root& other) : ptr_(other.ptr_) {
        if (ptr_) {
            ptr_->checkValid();
            insert();
        }
    }

    ~Root() {
        if (ptr_)
            remove();
    }

    operator T* () const { return ptr_; }
    T* operator->() { return ptr_; }
    const T* operator->() const { return ptr_; }
    const T* get() const { return ptr_; }
    T* get() { return ptr_; }

    Root& operator=(T* ptr) {
        if (ptr)
            ptr->checkValid();
        if (ptr_ && !ptr)
            remove();
        else if (!ptr_ && ptr)
            insert();
        ptr_ = ptr;
        return *this;
    }

    Root& operator=(const Root& other) {
        *this = other.ptr_;
    }

    virtual void trace(Tracer& t) { t.visit(&ptr_); }

  private:
    T* ptr_;
};

#endif
