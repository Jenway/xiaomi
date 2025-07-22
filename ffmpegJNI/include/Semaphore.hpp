#include <condition_variable>
#include <mutex>

// A simple counting semaphore implementation for use with pre-C++20
namespace player_utils {

class Semaphore {
public:
    explicit Semaphore(int count = 0)
        : count_(count)
    {
    }

    // V operation: Signal/release. Increments the count and wakes a waiting thread.
    void release()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        count_++;
        cv_.notify_one();
    }

    // P operation: Wait/acquire. Waits until the count is positive, then decrements it.
    void acquire()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // Wait until count_ > 0. The lambda predicate protects against spurious wakeups.
        cv_.wait(lock, [this]() { return count_ > 0; });
        count_--;
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    int count_;
};
}