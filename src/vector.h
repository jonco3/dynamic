#ifndef __VECTOR_H__
#define __VECTOR_H__

// Dynamically resizable vector with inline storage.
//
// todo:
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

template <typename T>
struct VectorStorage
{
    static const size_t InitialHeapCapacity = 16;
    static constexpr double GrowthFactor = 1.5;
    static constexpr double ShrinkThreshold = 0.5;

    VectorStorage()
      : size_(0),
        capacity_(0),
        inlineCapacity_(0),
        heapElements_(nullptr)
    {}

    template <size_t N>
    VectorStorage(uint8_t (&inlineData)[N])
      : size_(0),
        capacity_(N / sizeof(T)),
        inlineCapacity_(N / sizeof(T)),
        heapElements_(nullptr)
    {
        assert(static_cast<void*>(&inlineData[0]) ==
               static_cast<void*>(this + 1));
    }

    ~VectorStorage() {
        free(heapElements_);
    }

    size_t size() const {
        return size_;
    }

    size_t capacity() const {
        return capacity_;
    }

    void reserve(size_t newSize) {
        assert(capacity_ >= inlineCapacity_);
        assert(size_ <= capacity_);
        assert((capacity_ == inlineCapacity_) == (heapElements_ == nullptr));

        if (newSize <= capacity_)
            return;

        size_t heapCapacity = capacity_ - inlineCapacity_;
        size_t newHeapCapacity =
          heapCapacity != 0 ? heapCapacity : InitialHeapCapacity;
        while (newSize > newHeapCapacity + inlineCapacity_)
            newHeapCapacity = size_t(newHeapCapacity * GrowthFactor);
        assert(newHeapCapacity > heapCapacity);
        changeHeapCapacity(newHeapCapacity);
    }

  protected:
    const T* ptr(size_t i) const {
        assert(size_ <= capacity_);
        assert(i < size_);
        return i < inlineCapacity_ ? inlinePtr(i)
                                   : &heapElements_[i - inlineCapacity_];
    }

    T* ptr(size_t i) {
        assert(size_ <= capacity_);
        assert(i < size_);
        return i < inlineCapacity_ ? inlinePtr(i)
                                   : &heapElements_[i - inlineCapacity_];
    }

    T* incSize() {
        assert(size() < capacity());
        return ptr(size_++);
    }

    T* decSize() {
        assert(size_ != 0);
        T* result = ptr(size_ - 1);
        size_--;
        return result;
    }

    void maybeShrink() {
        if (capacity_ > inlineCapacity_ && size_ >= capacity_ * ShrinkThreshold)
            return;

        size_t newHeapCapacity = 0;
        if (size_ > inlineCapacity_) {
            newHeapCapacity = InitialHeapCapacity;
            while (newHeapCapacity + inlineCapacity_ < size_)
                newHeapCapacity *= GrowthFactor;
        }
        changeHeapCapacity(newHeapCapacity);
    }

  private:
    const T* inlinePtr(size_t i) const {
        assert(i < inlineCapacity_);
        assert(i < size_);
        const T* inlineElements = reinterpret_cast<const T*>(this + 1);
        return &inlineElements[i];
    }

    T* inlinePtr(size_t i) {
        assert(i < inlineCapacity_);
        assert(i < size_);
        T* inlineElements = reinterpret_cast<T*>(this + 1);
        return &inlineElements[i];
    }

    void changeHeapCapacity(size_t newHeapCapacity) {
        if (newHeapCapacity == capacity_ - inlineCapacity_)
            return;

        T* newHeapElements = nullptr;
        if (newHeapCapacity) {
            size_t bytes = sizeof(T) * newHeapCapacity;
            newHeapElements = static_cast<T*>(malloc(bytes));
        }
        if (size_ > inlineCapacity_) {
            assert(newHeapCapacity);
            for (size_t i = 0; i < size_ - inlineCapacity_; i++) {
                new (&newHeapElements[i])T(heapElements_[i]);
                heapElements_[i].~T();
            }
        }
        free(heapElements_);
        heapElements_ = newHeapElements;
        capacity_ = newHeapCapacity + inlineCapacity_;
    }

  private:
    size_t size_;
    size_t capacity_;
    size_t inlineCapacity_;
    T* heapElements_;
};

template <typename T>
struct Vector : public VectorStorage<T>
{
    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using difference_type = ptrdiff_t;
    using size_type = size_t;

    Vector() {}

    Vector(const Vector<T>& other) {
        copy(other);
    }

    template <typename S>
    Vector(size_t size, const S& fillValue) {
        fill(size, fillValue);
    }

    template <size_t N>
    Vector(uint8_t (&inlineData)[N])
      : VectorStorage<T>(inlineData)
    {}

    template <size_t N>
    Vector(uint8_t (&inlineData)[N], const Vector<T>& other)
      : VectorStorage<T>(inlineData)
    {
        copy(other);
    }

    template <size_t N, typename S>
    Vector(uint8_t (&inlineData)[N], size_t size, const S& fillValue)
      : VectorStorage<T>(inlineData)
    {
        fill(size, fillValue);
    }

    ~Vector() {
        while (size() != 0)
            destruct_back();
    }

    using Base = VectorStorage<T>;
    using Base::size;
    using Base::capacity;
    using Base::ptr;
    using Base::incSize;
    using Base::decSize;
    using Base::reserve;
    using Base::maybeShrink;

    bool empty() const {
        return size() == 0;
    }

    struct iterator
      : public VectorIteratorBase<Vector<T>, T, iterator>
    {
        using Base = VectorIteratorBase<Vector<T>, T, iterator>;

        iterator() : Base(nullptr, 0) {}

        iterator(const iterator& other)
          : Base(other.vector(), other.index()) {}

      private:
        iterator(Vector<T>* vector, size_t index)
          : Base(vector, index) {}

        friend Base;
        friend struct Vector<T>;
    };

    struct const_iterator
      : public VectorIteratorBase<const Vector<T>, const T, const_iterator>
    {
        using Base =
            VectorIteratorBase<const Vector<T>, const T, const_iterator>;

        const_iterator() : Base(nullptr, 0) {}

        const_iterator(const const_iterator& other)
          : Base(other.vector(), other.index()) {}

        const_iterator(const iterator& other)
          : Base(other.vector(), other.index()) {}

      private:
        const_iterator(const Vector* vector, size_t index)
          : Base(vector, index) {}

        friend Base;
        friend struct Vector<T>;
    };

    const_iterator begin() const noexcept {
        return const_iterator(this, 0);
    }

    iterator begin() noexcept {
        return iterator(this, 0);
    }

    const_iterator end() const noexcept {
        return const_iterator(this, size());
    }

    iterator end() noexcept {
        return iterator(this, size());
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
        return ref(size() - 1);
    }

    const T& back() const {
        assert(!empty());
        return ref(size() - 1);
    }

    void resize(size_t newSize) {
        if (size() == newSize)
            return;

        if (newSize < size()) {
            while (size() > newSize)
                destruct_back();
            maybeShrink();
            return;
        }

        reserve(newSize);
        while (size() < newSize)
            construct_back();
    }

    void push_back(const T& value) {
        reserve(size() + 1);
        construct_back(value);
    }

    void pop_back() {
        destruct_back();
        maybeShrink();
    }

    iterator erase(const_iterator pos) {
        return erase(pos, pos + 1);
    }

    iterator erase(const_iterator first, const_iterator last) {
        assert(first.vector() == this);
        assert(last.vector() == this);
        assert(first.index() <= last.index());
        assert(last.index() <= size());

        size_t count = last.index() - first.index();
        for (size_t i = last.index(); i < size(); i++)
            ref(i - count) = ref(i);
        for (size_t i = 0; i < count; i++)
            destruct_back();
        maybeShrink();
        return iterator(this, first.index());
    }

    iterator insert(const_iterator pos, const_reference value) {
        assert(pos.vector() == this);
        assert(pos.index() <= size());

        reserve(size() + 1);
        construct_back();
        for (size_t i = size(); i != pos.index() + 1; i--)
            ref(i - 1) = ref(i - 2);
        ref(pos.index()) = value;
        return iterator(this, pos.index());
    }

    iterator insert(const_iterator pos, const_iterator first, iterator last) {
        assert(pos.vector() == this);
        assert(pos.index() <= size());
        assert(first.vector() == last.vector());
        assert(first.index() <= last.index());
        assert(last.index() <= last.vector()->size());

        size_t count = last.index() - first.index();
        reserve(size() + count);
        for (size_t i = 0; i < count; i++)
            construct_back();
        for (size_t i = size(); i != pos.index() + count; i--)
            ref(i - 1) = ref(i - count - 1);
        auto source = first.vector();
        for (size_t i = 0; i < count; i++)
            ref(i + pos.index()) = source->ref(i + first.index());
        return iterator(this, pos.index());
    }

    template <class... Args>
    void emplace_back(Args&&... args) {
        reserve(size() + 1);
        construct_back(std::forward<Args>(args)...);
    }

  private:
    void checkIndex(size_t index) const {
        if (index >= size())
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
        new (incSize()) T(std::forward<Args>(args)...);
    }

    void destruct_back() {
        decSize()->~T();
    }

    void copy(const Vector<T>& other) {
        assert(size() == 0);
        reserve(other.size());
        for (size_t i = 0; i < other.size(); i++)
            construct_back(other.ref(i));
    }

    template <typename S>
    void fill(size_t newSize, const S& fillValue) {
        assert(size() == 0);
        reserve(newSize);
        for (size_t i = 0; i < newSize; i++)
            construct_back(fillValue);
    }
};

template <typename T, size_t N>
struct InlineVector : public Vector<T>
{
    InlineVector() : Vector<T>(inlineData_) {}

    InlineVector(const Vector<T>& other)
      : Vector<T>(inlineData_, other)
    {}

    template <typename S>
    InlineVector(size_t size, const S& fillValue)
      : Vector<T>(inlineData_, size, fillValue)
    {}

  private:
    uint8_t inlineData_[N * sizeof(T)];
};

#endif
