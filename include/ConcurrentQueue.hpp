#pragma once
#include "Semp.hpp"
#include <mutex>
#include <queue>

template <typename T>
class ConcurrentQueue {
public:
    explicit ConcurrentQueue(size_t capacity)
        : m_capacity(capacity)
        , m_empty_slots(capacity)
        , m_filled_slots(0)
    {
    }

    ConcurrentQueue(const ConcurrentQueue&) = delete;
    ConcurrentQueue& operator=(const ConcurrentQueue&) = delete;
    ConcurrentQueue(ConcurrentQueue&&) = delete;
    ConcurrentQueue& operator=(ConcurrentQueue&&) = delete;
    ~ConcurrentQueue() = default;

    void push(T&& item)
    {
        m_empty_slots.acquire();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_queue.push(std::move(item));
        }
        m_filled_slots.release();
    }

    T pop()
    {
        m_filled_slots.acquire();
        T item;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            item = std::move(m_queue.front());
            m_queue.pop();
        }
        m_empty_slots.release();
        return item;
    }

    size_t size() const
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

private:
    const size_t m_capacity;
    std::queue<T> m_queue;
    mutable std::mutex m_mutex; // mutable: for size()
    Semp<size_t> m_empty_slots;
    Semp<size_t> m_filled_slots;
};