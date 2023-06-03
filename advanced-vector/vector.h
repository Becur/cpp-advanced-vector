#pragma once
#include <cassert>
#include <cstdlib>
#include <new>
#include <utility>
#include <memory>
#include <algorithm>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
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

    RawMemory(const RawMemory&) = delete;
    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept 
    : buffer_(std::exchange(other.buffer_, nullptr))
    , capacity_(std::exchange(other.capacity_, 0)) {}
    RawMemory& operator=(RawMemory&& rhs) noexcept { 
        buffer_ = std::exchange(rhs.buffer_, nullptr);
        capacity_ = std::exchange(rhs.capacity_, 0);
        return *this;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
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
        , size_(size)  
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }
    
    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)  
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
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
    
    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }
    
    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        // Конструируем элементы в new_data, копируя их из data_
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        // Разрушаем элементы в data_
        std::destroy_n(data_.GetAddress(), size_);
        // Избавляемся от старой сырой памяти, обменивая её на новую
        data_.Swap(new_data);
        // При выходе из метода старая память будет возвращена в кучу
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                const size_t min_size = std::min(rhs.size_, size_);
                for(size_t i = 0; i < min_size; ++i){
                    (*this)[i] = rhs[i];
                }
                if(rhs.size_ < size_){
                    std::destroy_n(data_.GetAddress() + min_size, size_ - rhs.size_);
                }
                else{
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + min_size, rhs.size_ - size_, data_.GetAddress() + min_size);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector(Vector&& other) noexcept
    : data_(std::move(other.data_))
    , size_(std::exchange(other.size_, 0)){}

    Vector& operator=(Vector&& rhs) noexcept{
        data_ = std::move(rhs.data_);
        size_ = std::exchange(rhs.size_, 0);
        return *this;
    }

    void Swap(Vector& other) noexcept{
        if (this != &other) {
            data_.Swap(other.data_);
            std::swap(size_, other.size_);
        }
    }

    void Resize(size_t new_size){
        if(new_size < size_){
            std::destroy_n(data_ + new_size, size_ - new_size);
        }
        else{
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_ + size_, new_size - size_);
        }
        size_ = new_size;
    }
    void PushBack(const T& value){
        if (size_ < data_.Capacity()) {
            new (data_ + size_) T(value);
        }
        else{
            RawMemory<T> new_data((size_ == 0) ? 1 : (2 * size_));
            new (new_data + size_) T(value);
            // Конструируем элементы в new_data, копируя их из data_
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            // Разрушаем элементы в data_
            std::destroy_n(data_.GetAddress(), size_);
            // Избавляемся от старой сырой памяти, обменивая её на новую
            data_.Swap(new_data);
            // При выходе из метода старая память будет возвращена в кучу
        }
        ++size_;
    }
    void PushBack(T&& value){
        if (size_ < data_.Capacity()) {
            new (data_ + size_) T(std::move(value));
        }
        else{
            RawMemory<T> new_data((size_ == 0) ? 1 : (2 * size_));
            new (new_data + size_) T(std::move(value));
            // Конструируем элементы в new_data, копируя их из data_
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            // Разрушаем элементы в data_
            std::destroy_n(data_.GetAddress(), size_);
            // Избавляемся от старой сырой памяти, обменивая её на новую
            data_.Swap(new_data);
            // При выходе из метода старая память будет возвращена в кучу
        }
        ++size_;
    }
    void PopBack()  noexcept {
        --size_;
        std::destroy_at(data_ + size_);
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args){
        if (size_ < data_.Capacity()) {
            new (data_ + size_) T(std::forward<Args>(args)...);
        }
        else{
            RawMemory<T> new_data((size_ == 0) ? 1 : (2 * size_));
            new (new_data + size_) T(std::forward<Args>(args)...);
            // Конструируем элементы в new_data, копируя их из data_
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            // Разрушаем элементы в data_
            std::destroy_n(data_.GetAddress(), size_);
            // Избавляемся от старой сырой памяти, обменивая её на новую
            data_.Swap(new_data);
            // При выходе из метода старая память будет возвращена в кучу
        }
        ++size_;
        return *(data_ + size_ - 1);
    }

    using iterator = T*;
    using const_iterator = const T*;
    
    iterator begin() noexcept{
        return data_.GetAddress();
    }
    iterator end() noexcept{
        return data_ + size_;
    }
    const_iterator begin() const noexcept{
        return data_.GetAddress();
    }
    const_iterator end() const noexcept{
        return data_ + size_;
    }
    const_iterator cbegin() const noexcept{
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept{
        return data_ + size_;
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args){
        size_t pos_ = std::distance(cbegin(), pos);
        if(pos == cend()){
            EmplaceBack(std::forward<Args>(args)...);
            return data_ + pos_;
        }
        if (size_ < data_.Capacity()) {
            T value(std::forward<Args>(args)...);
            std::uninitialized_move(data_ + size_ - 1, data_ + size_, data_ + size_);
            std::move_backward(data_ + pos_, data_ + size_ - 1, data_ + size_);
            *(data_ + pos_) =  std::move(value);
        }
        else{
            RawMemory<T> new_data((size_ == 0) ? 1 : (2 * size_));
            new (new_data + pos_) T(std::forward<Args>(args)...);
            // Конструируем элементы в new_data, копируя их из data_
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), pos_, new_data.GetAddress());
                try{
                    std::uninitialized_move_n(data_.GetAddress() + pos_, size_ - pos_, new_data.GetAddress() + pos_ + 1);
                }
                catch(...){
                    std::destroy_n(new_data.GetAddress(), pos_);
                    throw;
                }
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), pos_, new_data.GetAddress());
                try{
                    std::uninitialized_copy_n(data_.GetAddress() + pos_, size_ - pos_, new_data.GetAddress() + pos_ + 1);
                }
                catch(...){
                    std::destroy_n(new_data.GetAddress(), pos_);
                    throw;
                }
            }
            // Разрушаем элементы в data_
            std::destroy_n(data_.GetAddress(), size_);
            // Избавляемся от старой сырой памяти, обменивая её на новую
            data_.Swap(new_data);
            // При выходе из метода старая память будет возвращена в кучу
        }
        ++size_;
        return data_ + pos_;
    }
    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/{
        size_t pos_ = std::distance(cbegin(), pos);
        std::move(data_ + pos_ + 1, data_ + size_, data_ + pos_);
        std::destroy_at(data_ + size_ - 1);
        --size_;
        return data_ + pos_;
    }
    iterator Insert(const_iterator pos, const T& value){
        return Emplace(pos, value);
    }
    iterator Insert(const_iterator pos, T&& value){
        return Emplace(pos, std::move(value));
    }
private:
    RawMemory<T> data_;
    size_t size_ = 0;
};