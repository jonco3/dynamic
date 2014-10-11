#ifndef __GC_H__
#define __GC_H__

#include <cassert>
#include <cstddef>
#include <cstdint>

#include <type_traits>

using namespace std;

// Simple mark and sweep garbage collector.

struct Cell;

// A visitor that visits edges in the object graph.
struct Tracer
{
    virtual void visit(Cell** cellp) = 0;

    // Convenience method to convert |DerivedCell* const *| to |Cell**|
    template <typename T>
    void visit(T*const* cellp) {
        static_assert(is_base_of<Cell, T>::value, "Type T must be derived from Cell");
        visit(reinterpret_cast<Cell**>(const_cast<T**>(cellp)));
    }
};

// Private base class for all garbage collected classes.
struct Cell
{
    Cell();
    virtual ~Cell();
    virtual void trace(Tracer& t) const = 0;

    bool shouldMark();
    bool shouldSweep();
    static bool mark(Cell** cellp);
    static bool sweep(Cell* cell);

  private:
    int8_t epoc_;
};

struct RootBase
{
  protected:
    static void add(Cell** cellp);
    static void remove(Cell** cellp);
};

// Roots a cell as long as it is alive.
template <typename T>
struct Root : public RootBase
{
    explicit inline Root(T* ptr = nullptr);
    inline ~Root();

    operator T* () const { return ptr_; }
    T* operator->() { return ptr_; }
    const T* operator->() const { return ptr_; }
    const T* get() const { return ptr_; }

    Root& operator=(T* ptr) {
        ptr_ = ptr;
        return *this;
    }

  private:
    T* ptr_;
};

namespace gc {
extern void collect();
extern size_t cellCount();
}

template <typename T>
Root<T>::Root(T* ptr) : ptr_(ptr)
{
    static_assert(is_base_of<Cell, T>::value, "Type T must be derived from Cell");
    add(reinterpret_cast<Cell**>(&ptr_));
}

template <typename T>
Root<T>::~Root()
{
    static_assert(is_base_of<Cell, T>::value, "Type T must be derived from Cell");
    remove(reinterpret_cast<Cell**>(&ptr_));
}

#endif
