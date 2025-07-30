#pragma once

#include "Demuxer.hpp"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <vector>

class MockPacketSink {
public:
    bool operator()(Demuxer::Packet& packet)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (packet.isData()) {
            packet_count_++;
            timestamps_.push_back(packet.get()->pts);
        } else if (packet.isFlush()) {
            flush_received_ = true;
            // =========================================================
            // FIX: 这就是修复测试的关键！
            // 收到 Flush 指令，意味着下游应该丢弃所有旧数据。
            // 我们清空 packet_count 和 timestamps，模拟这个行为。
            // =========================================================
            packet_count_ = 0;
            timestamps_.clear();
        } else if (packet.isEof()) {
            eof_received_ = true;
        }

        cv_.notify_all();
        return true;
    }

    // ... 其他函数保持不变 ...
    bool waitForPacketCount(size_t count, int timeout_ms = 2000)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&] {
            return packet_count_ >= count;
        });
    }

    bool waitForFlush(int timeout_ms = 1000)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&] {
            return flush_received_;
        });
    }

    bool waitForEof(int timeout_ms = 2000)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&] {
            return eof_received_;
        });
    }

    size_t getPacketCount()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return packet_count_;
    }

    void reset()
    {
        std::unique_lock<std::mutex> lock(mutex_);
        packet_count_ = 0;
        flush_received_ = false;
        eof_received_ = false;
        timestamps_.clear();
    }

    double getFirstPacketTimestamp(AVRational time_base)
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (timestamps_.empty())
            return -1.0;
        return timestamps_.front() * av_q2d(time_base);
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    size_t packet_count_ = 0;
    bool flush_received_ = false;
    bool eof_received_ = false;
    std::vector<int64_t> timestamps_;
};