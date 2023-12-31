#pragma once

#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& rhs) noexcept
        : buffer_(std::exchange(rhs.buffer_, nullptr))
        , capacity_(std::exchange(rhs.capacity_, 0)) {
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            buffer_ = std::move(rhs.buffer_);
            capacity_ = std::move(rhs.capacity_);
            rhs.buffer_ = nullptr;
            rhs.capacity_ = 0;
        }
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // ����������� �������� ����� ������ ������, ��������� �� ��������� ��������� �������
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // �������� ����� ������ ��� n ��������� � ���������� ��������� �� ��
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // ����������� ����� ������, ���������� ����� �� ������ buf ��� ������ Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size) {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_) {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_))
        , size_(std::exchange(other.size_, 0)) {
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ <= data_.Capacity()) {
                std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + std::min(size_, rhs.size_), data_.GetAddress());
                if (size_ <= rhs.size_) {
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }
                else {
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                }
                size_ = rhs.size_;
            }
            else {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        Swap(rhs);
        return *this;
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_), std::swap(size_, other.size_);
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) return;

        RawMemory<T> data(new_capacity);

        IsNothrowMoveOrCopyConstructible(std::move(data));

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(data);
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
        }
        else {
            if (new_size > data_.Capacity()) {
                const size_t new_capacity = std::max(data_.Capacity() * 2, new_size);
                Reserve(new_capacity);
            }
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
        }
        size_ = new_size;
    }

    void PushBack(T&& value) {
        EmplaceBack(std::forward<T&&>(value));
    }

    void PushBack(const T& value) {
        EmplaceBack(std::forward<const T&>(value));
    }

    void PopBack() noexcept {
        std::destroy_at(data_.GetAddress() + size_ - 1);
        --size_;
    }

    using iterator = T*;
    using const_iterator = const T*;

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator end() const noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }

    const_iterator cend() const noexcept {
        return data_.GetAddress() + size_;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        int diff = pos - begin();

        if (data_.Capacity() <= size_) {
            RegularInsert(diff, std::forward<Args>(args)...);
        }

        else {
            ReallocationInsert(diff, pos, std::forward<Args>(args)...);
        }

        ++size_;
        return begin() + diff;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        return *Emplace(end(), std::forward<Args&&>(args)...);
    }


    iterator Erase(const_iterator pos) {
        int diff = pos - begin();
        std::move(begin() + diff + 1, end(), begin() + diff);
        std::destroy_at(end() - 1);
        --size_;
        return begin() + diff;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;

    void IsNothrowMoveOrCopyConstructible(RawMemory<T>&& data) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, data.GetAddress());
        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, data.GetAddress());
        }
    }

    template <typename... Args>
    void RegularInsert(int diff, Args&&... args) {
        RawMemory<T> new_address(size_ == 0 ? 1 : size_ * 2);

        new (new_address.GetAddress() + diff) T(std::forward<Args>(args)...);

        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), diff, new_address.GetAddress());
            std::uninitialized_move_n(data_.GetAddress() + diff, size_ - diff, new_address.GetAddress() + diff + 1);

        }
        else {
            std::uninitialized_copy_n(data_.GetAddress(), diff, new_address.GetAddress());
            std::uninitialized_copy_n(data_.GetAddress() + diff, size_ - diff, new_address.GetAddress() + diff + 1);
        }

        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_address);
    }

    template <typename... Args>
    void ReallocationInsert(int diff, const_iterator pos, Args&&... args) {
        if (pos == end()) {
            new (end()) T(std::forward<Args>(args)...);
        }

        else {
            T tmp(std::forward<Args>(args)...);
            new (end()) T(std::forward<T>(data_[size_ - 1]));

            std::move_backward(begin() + diff, end() - 1, end());
            *(begin() + diff) = std::forward<T>(tmp);
        }
    }
};