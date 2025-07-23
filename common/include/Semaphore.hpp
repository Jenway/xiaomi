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

    // 禁止拷贝和赋值，因为它们不是线程安全的
    Semaphore(const Semaphore&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

    /**
     * @brief V operation: 释放一个信号量，将其计数值加一，并唤醒一个等待者。
     */
    void release()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        count_++;
        cv_.notify_one();
    }

    /**
     * @brief P operation: 阻塞地获取一个信号量，如果计数为0则等待。
     */
    void acquire()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // cv_.wait 会在 lambda 返回 false 时阻塞，并自动释放锁；
        // 当被唤醒时，它会重新获取锁并再次检查 lambda。
        cv_.wait(lock, [this]() { return count_ > 0 || shutdown_; });

        // acquire 后检查是否是由于 shutdown 被唤醒
        if (shutdown_) {
            return;
        }

        count_--;
    }

    // --- 新增方法 ---

    /**
     * @brief (新增) 尝试非阻塞地获取一个信号量。
     * @return 如果成功获取 (count > 0)，返回 true；否则立即返回 false。
     */
    bool try_acquire()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (count_ > 0) {
            count_--;
            return true;
        }
        return false;
    }

    /**
     * @brief (新增) 带超时的尝试获取。
     * @param timeout 等待的最长时间。
     * @return 如果在超时前成功获取，返回 true；否则返回 false。
     */
    template <typename Rep, typename Period>
    bool try_acquire_for(const std::chrono::duration<Rep, Period>& timeout)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (!cv_.wait_for(lock, timeout, [this]() { return count_ > 0 || shutdown_; })) {
            return false;
        }

        if (shutdown_) {
            return false; // 被 shutdown 唤醒，不算成功获取
        }

        count_--;
        return true;
    }

    /**
     * @brief (新增) 释放所有等待中的线程，用于优雅地关闭。
     *        这会设置一个 shutdown 标志并广播通知。
     */
    void release_all()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        shutdown_ = true;
        // 使用 notify_all() 来唤醒所有可能在 cv_.wait() 中阻塞的线程。
        cv_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    int64_t count_;
    bool shutdown_ = false; // 新增：用于优雅关闭的标志
};
} // namespace player_utils