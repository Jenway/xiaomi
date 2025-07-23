#pragma once
#include <array>
#include <stdexcept>

template <typename T, size_t N>
class RingBuffer {
public:
    RingBuffer() = default;

    void push(T&& value)
    {
        if (full()) {
            throw std::runtime_error("RingBuffer is full");
        }
        buffer_[write_] = std::move(value);
        write_ = (write_ + 1) % N;
        ++size_;
    }

    void pop()
    {
        if (empty()) {
            throw std::runtime_error("RingBuffer is empty");
        }
        read_ = (read_ + 1) % N;
        --size_;
    }

    T& front()
    {
        if (empty()) {
            throw std::runtime_error("RingBuffer is empty");
        }
        return buffer_[read_];
    }

    [[nodiscard]] bool empty() const { return size_ == 0; }
    [[nodiscard]] bool full() const { return size_ == N; }

private:
    std::array<T, N> buffer_;
    size_t read_ = 0;
    size_t write_ = 0;
    size_t size_ = 0;
};
