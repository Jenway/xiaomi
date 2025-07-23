#include <condition_variable>
#include <mutex>

namespace player_utils {

class Semaphore {
public:
    explicit Semaphore(int64_t count = 0)
        : count_(count)
    {
    }

    // V operation: Signal/release.
    void release()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        count_++;
        cv_.notify_one();
    }

    // P operation: Wait/acquire.
    void acquire()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this]() { return count_ > 0; });
        count_--;
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    int64_t count_;
};
} // namespace player_utils