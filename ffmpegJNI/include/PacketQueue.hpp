#pragma once
#include "Packet.hpp"
#include "Semaphore.hpp" // Assuming the class above is in this file
#include <mutex>
#include <queue>

namespace player_utils {

using counting_semaphore = Semaphore;

class PacketQueue {
public:
    explicit PacketQueue(size_t max_size)
        // Producer semaphore: counts the number of empty slots available.
        : empty_slots_(max_size)
        ,
        // Consumer semaphore: counts the number of filled slots available.
        filled_slots_(0)
    {
    }

    // Push a packet. Blocks if the queue is full.
    void push(ffmpeg_utils::Packet packet)
    {
        // 1. Wait until an empty slot is available.
        // This blocks if the queue is full (count of empty_slots is 0).
        empty_slots_.acquire();

        // 2. Lock the queue for exclusive access.
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_.push(std::move(packet));
        lock.unlock();

        // 3. Signal that one more filled slot is now available for a consumer.
        filled_slots_.release();
    }

    // Pop a packet. Blocks if the queue is empty.
    // NOTE: Shutdown handling is more complex with semaphores. See below.
    bool wait_and_pop(ffmpeg_utils::Packet& out_packet)
    {
        // 1. Wait until a filled slot is available.
        // This blocks if the queue is empty (count of filled_slots is 0).
        filled_slots_.acquire();

        // 2. Lock the queue for exclusive access.
        std::unique_lock<std::mutex> lock(queue_mutex_);

        // Check for shutdown signal after being woken up.
        if (shutdown_ && queue_.empty()) {
            // If shutting down and the queue is truly empty, we're done.
            // Release the lock and signal other consumers to unblock.
            lock.unlock();
            filled_slots_.release(); // Allow other consumers to wake up and exit
            return false;
        }

        out_packet = std::move(queue_.front());
        queue_.pop();
        lock.unlock();

        // 3. Signal that one more empty slot is now available for a producer.
        empty_slots_.release();
        return true;
    }

    void shutdown()
    {
        // Set the shutdown flag
        std::unique_lock<std::mutex> lock(queue_mutex_);
        shutdown_ = true;
        lock.unlock();

        // IMPORTANT: Unblock any waiting threads.
        // A consumer might be stuck waiting on filled_slots_.acquire().
        // We need to release it so it can wake up, check the shutdown flag, and exit.
        filled_slots_.release();
    }

private:
    std::mutex queue_mutex_;
    std::queue<ffmpeg_utils::Packet> queue_;
    bool shutdown_ = false;

    // Semaphores to manage state
    counting_semaphore empty_slots_;
    counting_semaphore filled_slots_;
};

} // namespace player_utils