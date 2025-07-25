#pragma once

#include "Semaphore.hpp"
#include <mutex>
#include <optional>
#include <queue>

namespace player_utils {

using counting_semaphore = Semaphore;

template <typename T, typename Container = std::queue<T>>
class SemQueue {
public:
    explicit SemQueue(size_t max_size)
        : max_size_(max_size)
        , empty_slots_(max_size)
        , filled_slots_(0)
    {
    }

    bool push(T element)
    {
        if (shutdown_)
            return false;
        empty_slots_.acquire();
        if (shutdown_)
            return false; // acquire 后再次检查，防止 shutdown 时死锁

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_.push(std::move(element));
        }

        filled_slots_.release();
        return true;
    }

    bool wait_and_pop(T& out_element)
    {
        if (shutdown_ && queue_is_empty_unsafe())
            return false;
        filled_slots_.acquire(); // 等待已填充槽位

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (shutdown_ && queue_.empty()) {
                // 如果在等待期间被 shutdown，需要把信号量还回去，以便其他等待者也能退出
                filled_slots_.release();
                return false;
            }
            out_element = std::move(queue_.front());
            queue_.pop();
        }

        empty_slots_.release();
        return true;
    }
    template <typename Rep, typename Period>
    bool wait_and_pop(T& out_element, const std::chrono::duration<Rep, Period>& timeout)
    {
        if (shutdown_ && queue_is_empty_unsafe()) {
            return false;
        }

        // 1. 带超时地等待一个 "已填充" 信号量
        if (!filled_slots_.try_acquire_for(timeout)) {
            // 如果等待超时，直接返回 false
            return false;
        }

        // 2. 成功获取信号量，现在可以安全地访问队列
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // 再次检查 shutdown 条件，防止在等待期间被关闭
            if (shutdown_ && queue_.empty()) {
                filled_slots_.release();
                return false;
            }
            out_element = std::move(queue_.front());
            queue_.pop();
        }

        empty_slots_.release();
        return true;
    }

    std::optional<T> front()
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        return queue_.front();
    }

    bool try_pop(T& out_element)
    {
        if (!filled_slots_.try_acquire()) {
            return false;
        }

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (shutdown_ && queue_.empty()) {
                filled_slots_.release();
                return false;
            }
            out_element = std::move(queue_.front());
            queue_.pop();
        }

        empty_slots_.release();
        return true;
    }

    void clear()
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        Container empty_queue;
        queue_.swap(empty_queue);
    }
    void reset()
    {
        clear();

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            shutdown_ = false;
        }

        while (filled_slots_.try_acquire())
            ; // 耗尽已填充信号
        while (empty_slots_.try_acquire())
            ; // 耗尽空槽位信号

        for (size_t i = 0; i < max_size_; ++i) {
            empty_slots_.release();
        }
    }
    void shutdown()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (shutdown_)
                return;
            shutdown_ = true;
        }

        filled_slots_.release_all();
        empty_slots_.release_all();
    }

    size_t size() const
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return queue_.size();
    }

    bool empty() const
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        return 0 == queue_.size();
    }

private:
    bool queue_is_empty_unsafe() const
    {
        return queue_.empty();
    }

    mutable std::mutex queue_mutex_;
    Container queue_;
    bool shutdown_ = false;
    const size_t max_size_;

    counting_semaphore empty_slots_;
    counting_semaphore filled_slots_;
};

} // namespace player_utils