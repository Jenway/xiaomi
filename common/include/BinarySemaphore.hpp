#pragma once

#include "Semaphore.hpp" // 包含你现有的计数信号量
#include <atomic>

namespace player_utils {

/**
 * @class BinarySemaphore
 * @brief 一个二进制信号量，作为计数信号量的适配器。
 *
 * 这个类封装了一个通用的计数信号量，但提供了严格的二进制行为（计数值只能是0或1）。
 * 它非常适合用于模拟需要“开/关”状态的门闩（Latch）或事件（Event）。
 * - release() 多次调用不会累积信号。
 * - acquire() 会消耗信号。
 */
class BinarySemaphore {
public:
    /**
     * @brief 构造一个二进制信号量。
     * @param initial_state 如果为 true，信号量初始状态为“有信号”(count=1)；否则为“无信号”(count=0)。
     */
    explicit BinarySemaphore(bool initial_state = false)
        : signaled_(initial_state)
        , semaphore_(initial_state ? 1 : 0) // 根据初始状态设置内部信号量的计数值
    {
    }

    // 禁止拷贝和赋值
    BinarySemaphore(const BinarySemaphore&) = delete;
    BinarySemaphore& operator=(const BinarySemaphore&) = delete;

    /**
     * @brief V operation (Signal/Release): 释放信号。
     *
     * 如果信号量当前没有信号，则将其置为有信号状态并唤醒一个等待者。
     * 如果信号量已经有信号，则此操作无效，直接返回。
     * 这确保了计数值永远不会超过1。
     */
    void release()
    {
        // 使用原子操作来确保线程安全
        // exchange(true) 会将 signaled_ 设置为 true，并返回它之前的值。
        bool was_signaled = signaled_.exchange(true, std::memory_order_acq_rel);

        // 只有当它之前是 false (无信号) 时，我们才需要释放底层的信号量。
        if (!was_signaled) {
            semaphore_.release();
        }
    }

    /**
     * @brief P operation (Wait/Acquire): 阻塞地获取信号。
     *
     * 如果有信号，则消耗该信号并立即返回。
     * 如果没有信号，则阻塞等待，直到有其他线程调用 release()。
     */
    void acquire()
    {
        // 等待并获取底层的信号量
        semaphore_.acquire();
        // 成功获取后，更新我们自己的状态标志
        signaled_ = false;
    }

    /**
     * @brief 尝试非阻塞地获取信号。
     * @return 如果成功获取信号返回 true，否则返回 false。
     */
    bool try_acquire()
    {
        if (semaphore_.try_acquire()) {
            signaled_ = false;
            return true;
        }
        return false;
    }

    /**
     * @brief 优雅地关闭，释放所有等待者。
     */
    void shutdown()
    {
        semaphore_.release_all();
    }

private:
    std::atomic<bool> signaled_; // 追踪二进制状态 (有信号/无信号)
    Semaphore semaphore_; // 被封装的、真正的计数信号量
};

} // namespace player_utils