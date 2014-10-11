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
extern void addRoot(Cell** cellp);
extern void removeRoot(Cell** cellp);
extern bool isDying(const Cell* cell);
} // namespace gc

// A visitor that visits edges in the object graph.
struct Tracer
{
    virtual void visit(Cell** cellp) = 0;

    // Convenience method to convert |DerivedCell* const *| to |Cell**|
    template <typename T>
    void visit(const T*const* cellp) {
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

// Roots a cell as long as it is alive.
template <typename T>
struct Root
{
    explicit inline Root(T* ptr = nullptr);
    inline ~Root();

    operator T* () const { return ptr_; }
    T* operator->() { return ptr_; }
    const T* operator->() const { return ptr_; }
    const T* get() const { return ptr_; }
    T* get() { return ptr_; }

    Root& operator=(T* ptr) {
        ptr_ = ptr;
        return *this;
    }

  private:
    T* ptr_;
};

namespace gc {

template <typename T>
void addRoot(T** cellp)
{
    static_assert(is_base_of<Cell, T>::value, "Type T must be derived from Cell");
    addRoot(reinterpret_cast<Cell**>(cellp));
}

template <typename T>
void removeRoot(T** cellp)

{
    static_assert(is_base_of<Cell, T>::value, "Type T must be derived from Cell");
    gc::removeRoot(reinterpret_cast<Cell**>(cellp));
}

} // namespace gc

template <typename T>
Root<T>::Root(T* ptr) : ptr_(ptr)
{
    gc::addRoot(&ptr_);
}

template <typename T>
Root<T>::~Root()
{
    gc::removeRoot(&ptr_);
}

#endif
