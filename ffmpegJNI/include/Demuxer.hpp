#pragma once

#include "MediaSource.hpp"
#include "Packet.hpp"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

class Demuxer {
public:
    using Packet = ffmpeg_utils::Packet;
    using PacketSink = std::function<bool(Packet&)>;

    explicit Demuxer(std::shared_ptr<MediaSource> source);
    ~Demuxer();

    Demuxer(const Demuxer&) = delete;
    Demuxer& operator=(const Demuxer&) = delete;

    void Start(PacketSink sink);
    void Stop();
    void Pause();
    void Resume();
    void SeekTo(double timestamp_sec);
    double GetDuration() const;

private:
    void run();

    std::shared_ptr<MediaSource> source_;
    PacketSink packet_sink_;
    std::thread demux_thread_;

    std::atomic<bool> stop_requested_ { false };
    std::atomic<bool> pause_requested_ { false };
    std::atomic<bool> seek_requested_ { false };
    std::atomic<double> seek_timestamp_sec_ { 0.0 };

    std::mutex mutex_;
    std::condition_variable cv_;
};
