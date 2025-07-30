#pragma once

#include <chrono>
#include <condition_variable>
#include <mutex>

namespace player_utils {

class Semaphore {
public:
    explicit Semaphore(int64_t count = 0)
        : count_(count)
    {
    }

    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

    void release()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        count_++;
        cv_.notify_one();
    }

    void acquire()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return count_ > 0 || shutdown_; });
        if (shutdown_) {
            return;
        }

        count_--;
    }

    bool try_acquire()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (count_ > 0) {
            count_--;
            return true;
        }
        return false;
    }

    template <typename Rep, typename Period>
    bool try_acquire_for(const std::chrono::duration<Rep, Period>& timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this]() { return count_ > 0 || shutdown_; })) {
            return false;
        }

        if (shutdown_) {
            return false;
        }

        count_--;
        return true;
    }

    void release_all()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        shutdown_ = true;
        cv_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    int64_t count_;
    bool shutdown_ = false;
};
} // namespace player_utils