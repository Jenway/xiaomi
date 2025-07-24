#include "Demuxer.hpp"
#include "MediaSource.hpp"
#include "Packet.hpp"
#include <iostream>

// 引入 FFmpeg 头文件
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/rational.h>
}

Demuxer::Demuxer(std::shared_ptr<MediaSource> source)
    : source_(std::move(source))
{
}

Demuxer::~Demuxer()
{
    if (demux_thread_.joinable()) {
        Stop();
    }
}

void Demuxer::Start(PacketSink sink)
{
    if (demux_thread_.joinable()) {
        return;
    }
    packet_sink_ = std::move(sink);
    stop_requested_.store(false);
    pause_requested_.store(false);
    seek_requested_.store(false);
    demux_thread_ = std::thread(&Demuxer::run, this);
}

void Demuxer::Stop()
{
    stop_requested_.store(true);
    cv_.notify_one();
    if (demux_thread_.joinable()) {
        demux_thread_.join();
    }
}

void Demuxer::Pause()
{
    pause_requested_.store(true);
}

void Demuxer::Resume()
{
    pause_requested_.store(false);
    cv_.notify_one();
}

bool Demuxer::SeekTo(double timestamp_sec)
{
    seek_timestamp_sec_.store(timestamp_sec);
    seek_requested_.store(true);
    cv_.notify_one();
    return true;
}

double Demuxer::GetDuration() const
{
    if (!source_) {
        return 0.0;
    }

    AVFormatContext* ctx = source_->get_format_context();
    if (!ctx || ctx->duration == AV_NOPTS_VALUE) {
        return 0.0;
    }
    return static_cast<double>(ctx->duration) / AV_TIME_BASE;
}

void Demuxer::run()
{
    std::cout << "[Demuxer Thread] Starting." << std::endl;

    AVFormatContext* ctx = source_->get_format_context();
    if (!ctx) {
        std::cerr << "[Demuxer Thread] Error: AVFormatContext is null." << std::endl;
        return;
    }

    bool discarding_packets_after_seek = false;
    double current_seek_time = 0.0;
    int video_idx = source_->get_video_stream_index();

    while (!stop_requested_.load()) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] {
                return !pause_requested_.load() || stop_requested_.load() || seek_requested_.load();
            });
        }

        if (stop_requested_.load())
            break;

        if (seek_requested_.exchange(false)) {
            current_seek_time = seek_timestamp_sec_.load();
            int64_t seek_target = static_cast<int64_t>(current_seek_time * AV_TIME_BASE);

            std::cout << "[Demuxer Thread] Seeking to " << current_seek_time << "s" << std::endl;
            int ret = av_seek_frame(ctx, -1, seek_target, AVSEEK_FLAG_BACKWARD);
            if (ret < 0) {
                std::cerr << "[Demuxer Thread] Error seeking: " << ret << std::endl;
            } else {
                avformat_flush(ctx);
                if (packet_sink_) {
                    Packet flush_packet = Packet::createFlushPacket();
                    packet_sink_(flush_packet);
                }
                discarding_packets_after_seek = true;
            }
            continue;
        }

        Packet packet;
        int ret = av_read_frame(ctx, packet.get());
        if (ret < 0) {
            // EOF or error: send EOF packet and exit
            if (packet_sink_) {
                Packet eof = Packet::createEofPacket();
                packet_sink_(eof);
            }
            break;
        }

        if (discarding_packets_after_seek) {
            if (video_idx >= 0 && packet.streamIndex() == video_idx) {
                AVRational tb = ctx->streams[video_idx]->time_base;
                double pts_sec = packet.get()->pts * av_q2d(tb);
                if (pts_sec < current_seek_time) {
                    continue; // still before seek target
                }
                // reached target, stop discarding
                discarding_packets_after_seek = false;
            } else if (video_idx >= 0) {
                continue; // drop non-video until first video at/after target
            }
        }

        if (packet_sink_) {
            if (!packet_sink_(packet)) {
                stop_requested_.store(true);
            }
        }
    }
    std::cout << "[Demuxer Thread] Stopped." << std::endl;
}
