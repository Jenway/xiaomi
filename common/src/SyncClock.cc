#include "SyncClock.hpp"
#include <android/log.h>

#define LOG_TAG "SyncClock"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

SyncClock::SyncClock() = default;

void SyncClock::update(double audio_pts)
{
    if (audio_pts >= 0) {
        master_clock_pts_ = audio_pts;
    }
}

double SyncClock::get() const
{
    return master_clock_pts_.load();
}

void SyncClock::reset(double position)
{
    master_clock_pts_ = position;
}

SyncClock::SyncDecision SyncClock::checkVideoFrame(double video_pts)
{
    double diff = video_pts - master_clock_pts_.load();
    if (diff > 0.01) { // 视频早了，需要等
        LOGI("SYNC: Video is early. VideoPTS=%.3f, AudioClock=%.3f, Diff=%.3f. Waiting...", video_pts, master_clock_pts_.load(), diff);
        return SyncDecision::Wait;
    }
    if (diff < -0.1) { // 视频晚太多，丢弃
        LOGW("SYNC: Video is too late. Dropping frame. VideoPTS=%.3f, AudioClock=%.3f, Diff=%.3f", video_pts, master_clock_pts_.load(), diff);
        return SyncDecision::Drop;
    }
    LOGI("SYNC: Submitting frame to renderer. VideoPTS=%.3f, AudioClock=%.3f, Diff=%.3f", video_pts, master_clock_pts_.load(), diff);
    return SyncDecision::Render; // 时间合适，渲染
}
