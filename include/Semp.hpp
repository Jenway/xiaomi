#pragma once
#include <condition_variable>
#include <cstdint>
#include <mutex>

template <typename T = int32_t>
class Semp {
private:
    T cnt;
    std::mutex mtx;
    std::condition_variable cv;

public:
    explicit Semp(T initial_count = 0)
        : cnt(initial_count)
    {
    }

    void acquire()
    {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return cnt > 0; });
        --cnt;
    }

    void release()
    {
        std::lock_guard<std::mutex> lock(mtx);
        ++cnt;
        cv.notify_one();
    }
};