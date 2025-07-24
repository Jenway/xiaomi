#pragma once

#include "Semaphore.hpp" // 假设你的 Semaphore.hpp 在这里
#include <mutex>
#include <optional> // C++17, 用于非阻塞获取
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

    // --- 生产者方法 ---

    /**
     * @brief 阻塞地将一个元素推入队列。如果队列已满，则等待。
     * @param element 要推入的元素。
     */
    bool push(T element)
    {
        if (shutdown_)
            return false;
        empty_slots_.acquire(); // 等待空槽位
        if (shutdown_)
            return false; // acquire 后再次检查，防止 shutdown 时死锁

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_.push(std::move(element));
        }

        filled_slots_.release(); // 释放一个已填充槽位
        return true;
    }

    // --- 消费者方法 ---

    /**
     * @brief 阻塞地等待并弹出一个元素。如果队列为空，则等待。
     * @param out_element 用于接收弹出元素的引用。
     * @return 如果成功弹出返回 true，如果队列被关闭且为空则返回 false。
     */
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

        empty_slots_.release(); // 释放一个空槽位
        return true;
    }
    template <typename Rep, typename Period>
    bool wait_and_pop(T& out_element, const std::chrono::duration<Rep, Period>& timeout)
    {
        if (shutdown_ && queue_is_empty_unsafe()) {
            return false;
        }

        // 1. 带超时地等待一个 "已填充" 信号量
        //    假设你的 Semaphore 提供了 try_acquire_for
        if (!filled_slots_.try_acquire_for(timeout)) {
            // 如果等待超时，直接返回 false
            return false;
        }

        // 2. 成功获取信号量，现在可以安全地访问队列
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            // 再次检查 shutdown 条件，防止在等待期间被关闭
            if (shutdown_ && queue_.empty()) {
                filled_slots_.release(); // 归还信号量，让其他线程也能退出
                return false;
            }
            out_element = std::move(queue_.front());
            queue_.pop();
        }

        // 3. 释放一个 "空槽位" 信号量
        empty_slots_.release();
        return true;
    }
    // --- 新增方法 ---

    /**
     * @brief (新增) 非阻塞地查看队列头部的元素，但不弹出。
     *        这是为渲染器设计的，用于检查下一帧的PTS。
     * @return 如果队列非空，返回指向头部元素的可选值；否则返回 std::nullopt。
     */
    std::optional<T> front()
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (queue_.empty()) {
            return std::nullopt;
        }
        // 返回一个拷贝或引用，取决于T的类型和使用场景
        // 对于 shared_ptr<VideoFrame>，拷贝是廉价的
        return queue_.front();
    }

    /**
     * @brief (新增) 非阻塞地尝试弹出一个元素。
     *        用于渲染器丢弃过期的帧。
     * @param out_element 用于接收弹出元素的引用。
     * @return 如果成功弹出一个元素，返回 true；如果队列为空，立即返回 false。
     */
    bool try_pop(T& out_element)
    {
        // 尝试非阻塞地获取一个填充信号量
        if (!filled_slots_.try_acquire()) {
            return false; // 队列为空，没有可弹出的元素
        }

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (shutdown_ && queue_.empty()) {
                filled_slots_.release(); // 归还信号量
                return false;
            }
            out_element = std::move(queue_.front());
            queue_.pop();
        }

        empty_slots_.release();
        return true;
    }

    /**
     * @brief (新增) 清空整个队列。
     *        用于 Seek 操作。
     */
    void clear()
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);

        // 1. 获取队列中当前的元素数量
        size_t items_to_clear = queue_.size();

        // 2. 清空内部容器
        Container empty_queue;
        queue_.swap(empty_queue); // 高效清空

        // 3. 调整信号量，这是最关键的一步
        // 我们不能简单地将 filled_slots_ 置为 0，因为可能有线程正在 acquire 它。
        // 正确的做法是：我们“消耗”了多少个 filled_slots，就要“返还”多少个 empty_slots。
        for (size_t i = 0; i < items_to_clear; ++i) {
            // 尝试获取一个已填充的信号量，这会减少 filled_slots_ 的计数
            // 因为我们已经锁住了队列，所以这个 acquire 应该会成功（除非有bug）
            filled_slots_.try_acquire();
            // 归还一个空槽位信号量
            empty_slots_.release();
        }
    }

    // --- 控制方法 ---

    /**
     * @brief 关闭队列。唤醒所有等待的线程，使其退出。
     */
    void shutdown()
    {
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            if (shutdown_)
                return;
            shutdown_ = true;
        }

        // 释放所有潜在的等待者
        // 释放一个 filled_slots_ 来唤醒消费者
        filled_slots_.release_all();
        // 释放一个 empty_slots_ 来唤醒生产者
        empty_slots_.release_all(); // 假设你的 Semaphore 有 release_all
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
        // 在已有锁的情况下调用
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