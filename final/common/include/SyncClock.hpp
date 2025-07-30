#pragma once
#include <atomic>
#include <cstdint>

class SyncClock {
public:
    SyncClock();

    void update(double audio_pts);

    double get() const;
    void reset(double position = 0.0);
    // 同步决策
    enum class SyncDecision : uint8_t {
        Render, // 渲染
        Drop, // 丢弃
        Wait // 等待
    };

    SyncDecision checkVideoFrame(double video_pts);

private:
    std::atomic<double> master_clock_pts_ { 0.0 };
    std::atomic<int> frame_counter_ { 0 };
};