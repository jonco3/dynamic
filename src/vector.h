#ifndef __VECTOR_H__
#define __VECTOR_H__

// Dynamically resizable vector with inline storage.
//
// todo:
//  - allow inline memory to be passed in constructor
//  - GC allocate the heap memory
//  - memcpy where possible

#include "assert.h"

#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <utility>

template <typename V, typename T, typename Derived>
struct VectorIteratorBase
{
    using difference_type = ptrdiff_t;
    using value_type = T;
    using reference = T&;
    using pointer = T*;
    using iterator_category = std::random_access_iterator_tag;

    VectorIteratorBase()
      : vector_(nullptr), index_(0) {}

    VectorIteratorBase(V* vector, size_t index)
      : vector_(vector), index_(index)
    {
        checkSize();
    }

    VectorIteratorBase(const VectorIteratorBase& other)
      : vector_(other.vector_), index_(other.index_) {}

    V* vector() const {
        return vector_;
    }

    size_t index() const {
        return index_;
    }

    bool operator==(const VectorIteratorBase& other) const {
        return vector_ == other.vector_ && index_ == other.index_;
    }

    bool operator!=(const VectorIteratorBase& other) const {
        return !(*this == other);
    }

    bool operator<(const VectorIteratorBase& other) const {
        checkCompare(other);
        return index_ < other.index();
    }

    bool operator<=(const VectorIteratorBase& other) const {
        checkCompare(other);
        return index_ <= other.index();
    }

    bool operator>(const VectorIteratorBase& other) const {
        checkCompare(other);
        return index_ > other.index();
    }

    bool operator>=(const VectorIteratorBase& other) const {
        checkCompare(other);
        return index_ >= other.index();
    }

    reference operator*() const {
        checkDeref();
        return (*vector_)[index_];
    }

    reference operator->() const {
        checkDeref();
        return (*vector_)[index_];
    }

    Derived& operator++() {
        index_++;
        checkSize();
        return *derived();
    }

    Derived operator++(int) {
        Derived result(*derived());
        ++(*this);
        return result;
    }

    Derived& operator--() {
        index_--;
        checkSize();
        return *derived();
    }

    Derived operator+(size_t x) {
        return Derived(vector_, index_ + x);
    }

    Derived& operator+=(size_t x) {
        index_ += x;
        checkSize();
        return *derived();
    }

    Derived operator-(size_t x) {
        return Derived(vector_, index_ - x);
    }

    difference_type operator-(const Derived& other) {
        return index_ - other.index();
    }

    Derived& operator-=(size_t x) {
        index_ -= x;
        checkSize();
        return *derived();
    }

  private:
    V* vector_;
    size_t index_;

    Derived* derived() {
        return static_cast<Derived*>(this);
    }

    void checkDeref() const {
        assert(vector_);
    }

    void checkSize() const {
        checkDeref();
        assert(index_ <= vector_->size());
    }

    void checkCompare(const VectorIteratorBase& other) const {
        checkSize();
        other.checkSize();
        assert(vector_ == other.vector());
    }
};

template <typename T, size_t N = 0>
struct Vector
{
    static const size_t InitialHeapCapacity = 16;
    static constexpr double GrowthFactor = 1.5;

    Vector()
      : size_(0),
        capacity_(N),
        heapElements_(nullptr)
    {}

    Vector(const Vector<T, N>& other)
      : size_(0),
        capacity_(N),
        heapElements_(nullptr)
    {
        reserve(other.size());
        for (size_t i = 0; i < other.size(); i++)
            construct_back(other.ref(i));
    }

    template <typename S>
    Vector(size_t size, const S& fillValue)
      : size_(0),
        capacity_(N),
        heapElements_(nullptr)
    {
        reserve(size);
        for (size_t i = 0; i < size; i++)
            construct_back(fillValue);
    }

    ~Vector() {
        while (size_ != 0)
            destruct_back();
        free(heapElements_);
    }

    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using difference_type = ptrdiff_t;
    using size_type = size_t;

    struct iterator
      : public VectorIteratorBase<Vector<T, N>, T, iterator>
    {
        using Base = VectorIteratorBase<Vector<T, N>, T, iterator>;

        iterator() : Base(nullptr, 0) {}

        iterator(const iterator& other)
          : Base(other.vector(), other.index()) {}

      private:
        iterator(Vector<T, N>* vector, size_t index)
          : Base(vector, index) {}

        friend Base;
        friend struct Vector<T, N>;
    };

    struct const_iterator
      : public VectorIteratorBase<const Vector<T, N>, const T, const_iterator>
    {
        using Base =
            VectorIteratorBase<const Vector<T, N>, const T, const_iterator>;

        const_iterator() : Base(nullptr, 0) {}

        const_iterator(const const_iterator& other)
          : Base(other.vector(), other.index()) {}

        const_iterator(const iterator& other)
          : Base(other.vector(), other.index()) {}

      private:
        const_iterator(const Vector* vector, size_t index)
          : Base(vector, index) {}

        friend Base;
        friend struct Vector<T, N>;
    };

    const_iterator begin() const noexcept {
        return const_iterator(this, 0);
    }

    iterator begin() noexcept {
        return iterator(this, 0);
    }

    const_iterator end() const noexcept {
        return const_iterator(this, size_);
    }

    iterator end() noexcept {
        return iterator(this, size_);
    }

    size_t size() const {
        return size_;
    }

    bool empty() const {
        return size_ == 0;
    }

    size_t capacity() const {
        return capacity_;
    }

    const T& operator[](size_t i) const {
        return ref(i);
    }

    T& operator[](size_t i) {
        return ref(i);
    }

    const T& at(size_t i) const {
        checkIndex(i);
        return ref(i);
    }

    T& at(size_t i) {
        checkIndex(i);
        return ref(i);
    }

    T& front() {
        assert(!empty());
        return ref(0);
    }

    const T& front() const {
        assert(!empty());
        return ref(0);
    }

    T& back() {
        assert(!empty());
        return ref(size_ - 1);
    }

    const T& back() const {
        assert(!empty());
        return ref(size_ - 1);
    }

    void resize(size_t size) {
        if (size_ == size)
            return;

        if (size < size_) {
            // todo: maybe shrink storage
            while (size_ > size)
                destruct_back();
            return;
        }

        reserve(size);
        while (size_ < size)
            construct_back();
    }

    void push_back(const T& value) {
        reserve(size_ + 1);
        construct_back(value);
    }

    void pop_back() {
        destruct_back();
        // todo: maybe shrink
    }

    iterator erase(const_iterator pos) {
        return erase(pos, pos + 1);
    }

    iterator erase(const_iterator first, const_iterator last) {
        assert(first.vector() == this);
        assert(last.vector() == this);
        assert(first.index() <= last.index());
        assert(last.index() <= size_);

        size_t count = last.index() - first.index();
        for (size_t i = last.index(); i < size_; i++)
            ref(i - count) = ref(i);
        for (size_t i = 0; i < count; i++)
            destruct_back();
        // todo: maybe shrink
        return iterator(this, first.index());
    }

    iterator insert(const_iterator pos, const_reference value) {
        assert(pos.vector() == this);
        assert(pos.index() <= size_);

        reserve(size_ + 1);
        construct_back();
        for (size_t i = size_; i != pos.index() + 1; i--)
            ref(i - 1) = ref(i - 2);
        ref(pos.index()) = value;
        return iterator(this, pos.index());
    }

    iterator insert(const_iterator pos, const_iterator first, iterator last) {
        assert(pos.vector() == this);
        assert(pos.index() <= size_);
        assert(first.vector() == last.vector());
        assert(first.index() <= last.index());
        assert(last.index() <= last.vector()->size());

        size_t count = last.index() - first.index();
        reserve(size_ + count);
        for (size_t i = 0; i < count; i++)
            construct_back();
        for (size_t i = size_; i != pos.index() + count; i--)
            ref(i - 1) = ref(i - count - 1);
        auto source = first.vector();
        for (size_t i = 0; i < count; i++)
            ref(i + pos.index()) = source->ref(i + first.index());
        return iterator(this, pos.index());
    }

    template <class... Args>
    void emplace_back(Args&&... args) {
        reserve(size_ + 1);
        construct_back(std::forward<Args>(args)...);
    }

  private:
    size_t size_;
    size_t capacity_;
    T* heapElements_;
    uint8_t inlineData_[N * sizeof(T)];

    void checkIndex(size_t index) const {
        if (index >= size_)
            throw std::out_of_range("Vector index out of range");
    }

    const T& ref(size_t i) const {
        return *ptr(i);
    }

    T& ref(size_t i)  {
        return *ptr(i);
    }

    template <class... Args>
    void construct_back(Args&&... args) {
        assert(size_ < capacity_);
        size_++;
        new (ptr(size_ - 1)) T(std::forward<Args>(args)...);
    }

    void destruct_back() {
        assert(size_ != 0);
        ref(size_ - 1).~T();
        size_--;
    }

    const T* ptr(size_t i) const {
        assert(size_ <= capacity_);
        assert(i < size_);
        return i < N ? inlinePtr(i) : &heapElements_[i - N];
    }

    T* ptr(size_t i) {
        assert(size_ <= capacity_);
        assert(i < size_);
        return i < N ? inlinePtr(i) : &heapElements_[i - N];
    }

    const T* inlinePtr(size_t i) const {
        assert(i < N);
        assert(i < size_);
        return reinterpret_cast<const T*>(&inlineData_[i * sizeof(T)]);
    }

    T* inlinePtr(size_t i) {
        assert(i < N);
        assert(i < size_);
        return reinterpret_cast<T*>(&inlineData_[i * sizeof(T)]);
    }

    void reserve(size_t newSize) {
        assert(capacity_ >= N);
        assert(size_ <= capacity_);
        assert((capacity_ == N) == (heapElements_ == nullptr));

        if (newSize <= capacity_)
            return;

        size_t heapCapacity = capacity_ - N;
        size_t newHeapCapacity =
          heapCapacity != 0 ? heapCapacity : InitialHeapCapacity;
        while (newSize > newHeapCapacity + N)
            newHeapCapacity = size_t(newHeapCapacity * GrowthFactor);
        assert(newHeapCapacity > heapCapacity);
        T* newHeapElements =
          static_cast<T*>(malloc(sizeof(T) * newHeapCapacity));
        if (size_ > N) {
            for (size_t i = 0; i < size_ - N; i++) {
                new (&newHeapElements[i])T(heapElements_[i]);
                heapElements_[i].~T();
            }
        }
        free(heapElements_);
        heapElements_ = newHeapElements;
        capacity_ = newHeapCapacity + N;
    }
};

#endif
