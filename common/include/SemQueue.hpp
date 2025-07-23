#pragma once
#include "Semaphore.hpp"
#include <mutex>
#include <queue>

namespace player_utils {

using counting_semaphore = Semaphore;

template <typename T>
class SemQueue {
public:
    explicit SemQueue(size_t max_size)
        : empty_slots_(max_size)
        , filled_slots_(0)
    {
    }

    void push(T packet)
    {
        empty_slots_.acquire();

        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_.push(std::move(packet));
        lock.unlock();

        filled_slots_.release();
    }

    bool wait_and_pop(T& out_packet)
    {

        filled_slots_.acquire();
        std::unique_lock<std::mutex> lock(queue_mutex_);

        if (shutdown_ && queue_.empty()) {
            lock.unlock();
            filled_slots_.release();
            return false;
        }

        out_packet = std::move(queue_.front());
        queue_.pop();
        lock.unlock();

        empty_slots_.release();
        return true;
    }

    void shutdown()
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        shutdown_ = true;
        lock.unlock();

        filled_slots_.release();
    }

private:
    std::mutex queue_mutex_;
    std::queue<T> queue_;
    bool shutdown_ = false;

    counting_semaphore empty_slots_;
    counting_semaphore filled_slots_;
};

} // namespace player_utils