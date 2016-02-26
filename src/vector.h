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
struct VectorStorageBase
{
    VectorStorageBase()
      : size_(0),
        capacity_(0),
        heapElements_(nullptr)
    {}

    ~VectorStorageBase() {
        free(heapElements_);
    }

    size_t size() const {
        return size_;
    }

    size_t capacity() const {
        return capacity_;
    }

  protected:
    size_t size_;
    size_t capacity_;
    T* heapElements_;
};

template <typename T>
struct VectorStorageHeap : public VectorStorageBase<T>
{
    using Base = VectorStorageBase<T>;
    using Base::size;
    using Base::capacity;

  protected:
    using Base::heapElements_;

    size_t inlineCapacity() const {
        return 0;
    }

    size_t heapCapacity() const {
        return capacity();
    }

    const T* ptr(size_t i) const {
        assert(i < capacity());
        return &heapElements_[i];
    }

    T* ptr(size_t i) {
        assert(i < capacity());
        return &heapElements_[i];
    }
};

template <typename T>
struct VectorStorageInline : public VectorStorageBase<T>
{
    VectorStorageInline()
      : inlineCapacity_(0)
    {}

    using Base = VectorStorageBase<T>;
    using Base::size;
    using Base::capacity;

    template <size_t N>
    void initInlineData(uint8_t (&inlineData)[N]) {
        assert(capacity() == 0);
        assert(inlineCapacity_ == 0);
        assert(static_cast<void*>(&inlineData[0]) ==
               static_cast<void*>(this + 1));
        this->inlineCapacity_ = N / sizeof(T);
        this->capacity_ = inlineCapacity_;
    }

  protected:
    using Base::heapElements_;

    size_t inlineCapacity() const {
        return inlineCapacity_;
    }

    size_t heapCapacity() const {
        assert(capacity() >= inlineCapacity_);
        return capacity() - inlineCapacity_;
    }

    const T* ptr(size_t i) const {
        assert(i < size());
        return i < inlineCapacity_ ? inlinePtr(i)
                                   : heapPtr(i - inlineCapacity_);
    }

    T* ptr(size_t i) {
        assert(i < size());
        return i < inlineCapacity_ ? inlinePtr(i)
                                   : heapPtr(i - inlineCapacity_);
    }

  private:
    size_t inlineCapacity_;

    const T* inlinePtr(size_t i) const {
        assert(i < inlineCapacity_);
        assert(inlineCapacity_ <= capacity());
        const T* inlineElements = reinterpret_cast<const T*>(this + 1);
        return &inlineElements[i];
    }

    T* inlinePtr(size_t i) {
        assert(i < inlineCapacity_);
        assert(inlineCapacity_ <= capacity());
        T* inlineElements = reinterpret_cast<T*>(this + 1);
        return &inlineElements[i];
    }

    const T* heapPtr(size_t i) const {
        assert(i < heapCapacity());
        return &heapElements_[i];
    }

    T* heapPtr(size_t i) {
        assert(i < heapCapacity());
        return &heapElements_[i];
    }
};

template <typename T, typename VectorStorage>
struct VectorImpl : public VectorStorage
{
    using Self = VectorImpl<T, VectorStorage>;
    using Base = VectorStorage;

    using value_type = T;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;
    using difference_type = ptrdiff_t;
    using size_type = size_t;

    static const size_t InitialHeapCapacity = 16;

    VectorImpl() {}

    VectorImpl(const Self& other) {
        copy(other);
    }

    template <typename S>
    VectorImpl(size_t size, const S& fillValue) {
        fill(size, fillValue);
    }

    ~VectorImpl() {
        while (size() != 0)
            destruct_back();
    }

    using Base::size;
    using Base::capacity;

    struct iterator
      : public VectorIteratorBase<Self, T, iterator>
    {
        using Base = VectorIteratorBase<Self, T, iterator>;

        iterator() : Base(nullptr, 0) {}

        iterator(const iterator& other)
          : Base(other.vector(), other.index()) {}

      private:
        iterator(Self* vector, size_t index)
          : Base(vector, index) {}

        friend Base;
        friend Self;
    };

    struct const_iterator
      : public VectorIteratorBase<const Self, const T, const_iterator>
    {
        using Base =
            VectorIteratorBase<const Self, const T, const_iterator>;

        const_iterator() : Base(nullptr, 0) {}

        const_iterator(const const_iterator& other)
          : Base(other.vector(), other.index()) {}

        const_iterator(const iterator& other)
          : Base(other.vector(), other.index()) {}

      private:
        const_iterator(const Self* vector, size_t index)
          : Base(vector, index) {}

        friend Base;
        friend Self;
    };

    bool empty() const {
        return size() == 0;
    }

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
        if (newSize > size()) {
            reserve(newSize);
            while (size() < newSize)
                construct_back();
            return;
        }

        if (newSize < size()) {
            while (size() > newSize)
                destruct_back();
            return;
        }
    }

    void reserve(size_t newSize) {
        assert(capacity() >= inlineCapacity());
        assert(size() <= capacity());
        assert((capacity() == inlineCapacity()) ==
               (this->heapElements_ == nullptr));

        if (newSize <= capacity())
            return;

        size_t newHeapCapacity =
            heapCapacity() != 0 ? heapCapacity() : InitialHeapCapacity;
        while (newSize > newHeapCapacity + inlineCapacity())
            newHeapCapacity += newHeapCapacity / 2;
        assert(newHeapCapacity > heapCapacity());
        changeHeapCapacity(newHeapCapacity);
    }

    void push_back(const T& value) {
        reserve(size() + 1);
        construct_back(value);
    }

    void pop_back() {
        destruct_back();
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

    void shrink_to_fit() {
        assert(capacity() >= inlineCapacity());

        if (heapCapacity() == 0 ||
            (heapCapacity() == InitialHeapCapacity && size() > inlineCapacity()))
        {
            return;
        }

        size_t newHeapCapacity = 0;
        if (size() > inlineCapacity()) {
            newHeapCapacity = InitialHeapCapacity;
            while (newHeapCapacity + inlineCapacity() < size())
                newHeapCapacity += newHeapCapacity / 2;
        }

        if (newHeapCapacity != heapCapacity())
            changeHeapCapacity(newHeapCapacity);
    }

    // todo: turn this into assign
    void copy(const Self& other) {
        assert(size() == 0);
        reserve(other.size());
        for (size_t i = 0; i < other.size(); i++)
            construct_back(other.ref(i));
    }

    // todo: turn this into assign
    template <typename S>
    void fill(size_t newSize, const S& fillValue) {
        assert(size() == 0);
        reserve(newSize);
        for (size_t i = 0; i < newSize; i++)
            construct_back(fillValue);
    }

  private:
    using Base::inlineCapacity;
    using Base::heapCapacity;
    using Base::ptr;

    void checkIndex(size_t index) const {
        if (index >= size())
            throw std::out_of_range("Vector index out of range");
    }

    const T& ref(size_t i) const {
        assert(i < size());
        return *ptr(i);
    }

    T& ref(size_t i)  {
        assert(i < size());
        return *ptr(i);
    }

    T* incSize() {
        assert(size() < capacity());
        return ptr(this->size_++);
    }

    T* decSize() {
        assert(size() != 0);
        T* result = ptr(size() - 1);
        this->size_--;
        return result;
    }

    template <class... Args>
    void construct_back(Args&&... args) {
        new (incSize()) T(std::forward<Args>(args)...);
    }

    void destruct_back() {
        decSize()->~T();
    }

    void changeHeapCapacity(size_t newHeapCapacity) {
        assert(newHeapCapacity != heapCapacity());

        T* newHeapElements = nullptr;
        if (newHeapCapacity) {
            size_t bytes = sizeof(T) * newHeapCapacity;
            newHeapElements = static_cast<T*>(malloc(bytes));
        }
        if (size() > inlineCapacity()) {
            assert(newHeapCapacity);
            for (size_t i = 0; i < size() - inlineCapacity(); i++) {
                new (&newHeapElements[i])T(this->heapElements_[i]);
                this->heapElements_[i].~T();
            }
        }
        free(this->heapElements_);
        this->heapElements_ = newHeapElements;
        this->capacity_ = newHeapCapacity + inlineCapacity();
    }
};

template <typename T>
struct Vector : public VectorImpl<T, VectorStorageHeap<T>>
{
    using Base = VectorImpl<T, VectorStorageHeap<T>>;

    Vector() {}

    Vector(const Vector<T>& other) : Base(other) {}

    template <typename S>
    Vector(size_t size, const S& fillValue) : Base(size, fillValue) {}
};

template <typename T>
using InlineVectorBase = VectorImpl<T, VectorStorageInline<T>>;

template <typename T, size_t N>
    struct InlineVector : public InlineVectorBase<T>
{
    using Base = VectorImpl<T, VectorStorageInline<T>>;

    InlineVector() {
        this->initInlineData(inlineData_);
    }

    InlineVector(const Vector<T>& other) {
        this->initInlineData(inlineData_);
        this->copy(other);
    }

    template <typename S>
    InlineVector(size_t size, const S& fillValue)
    {
        this->initInlineData(inlineData_);
        this->fill(size, fillValue);
    }

  private:
    uint8_t inlineData_[N * sizeof(T)];
};

#endif
