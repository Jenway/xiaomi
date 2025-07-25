#include "Demuxer.hpp"
#include "MediaSource.hpp"
#include <iostream>

// 引入 FFmpeg 头文件
extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/rational.h>
}
#include <android/log.h>

#define LOG_TAG "Mp4Parser Demuxer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#include <sstream> // Required for stringstream
#include <thread> // Required for this_thread

static void log_seek_message(int stream_index, double time_sec, int64_t target_ts)
{
    std::stringstream ss;
    ss << "Demuxer: Seeking stream " << stream_index
       << " to time " << time_sec << " (timestamp " << target_ts << ")";
    LOGI("%s", ss.str().c_str());
}

static void log_thread_entry()
{
    std::stringstream ss;
    ss << ">>> Demux thread [ID: " << std::this_thread::get_id() << "] entered.";
    LOGI("%s", ss.str().c_str());
}

static void log_thread_exit()
{
    std::stringstream ss;
    ss << "<<< Demux thread [ID: " << std::this_thread::get_id() << "] is exiting.";
    LOGI("%s", ss.str().c_str());
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

void Demuxer::SeekTo(double time_sec)
{
    if (!source_ || !source_->get_format_context()) {
        return;
    }

    auto* format_context = source_->get_format_context();
    int stream_index = source_->get_video_stream_index(); // 通常以视频流为基准 seek
    if (stream_index < 0) {
        stream_index = source_->get_audio_stream_index(); // 如果没有视频，用音频
    }
    if (stream_index < 0) {
        LOGE("Demuxer::SeekTo: No valid stream to seek on.");
        return;
    }

    auto* stream = format_context->streams[stream_index];

    // 将秒转换为流的内部时间基（time_base）
    int64_t target_ts = time_sec / av_q2d(stream->time_base);

    log_seek_message(stream_index, time_sec, target_ts);

    // av_seek_frame 是一个复杂的函数。
    // AVSEEK_FLAG_BACKWARD 标志意味着它会 seek 到目标时间戳之前的最近的一个关键帧（keyframe）。
    // 这是最常用、最稳妥的方式。
    int ret = av_seek_frame(format_context, stream_index, target_ts, AVSEEK_FLAG_BACKWARD);

    if (ret < 0) {
        LOGE("Demuxer: av_seek_frame failed with error: %s", av_err2str(ret));
    } else {
        LOGI("Demuxer: av_seek_frame successful.");
    }
}

double Demuxer::GetDuration() const
{
    if (!source_) {
        return 0.0;
    }

    AVFormatContext* ctx = source_->get_format_context();
    if ((ctx == nullptr) || ctx->duration == AV_NOPTS_VALUE) {
        return 0.0;
    }
    return static_cast<double>(ctx->duration) / AV_TIME_BASE;
}

// void Demuxer::run()
// {
//     log_thread_entry();

//     AVFormatContext* ctx = source_->get_format_context();
//     if (!ctx) {
//         std::cerr << "[Demuxer Thread] Error: AVFormatContext is null." << std::endl;
//         return;
//     }

//     bool discarding_packets_after_seek = false;
//     double current_seek_time = 0.0;
//     int video_idx = source_->get_video_stream_index();

//     while (!stop_requested_.load()) {
//         {
//             std::unique_lock<std::mutex> lock(mutex_);
//             cv_.wait(lock, [this] {
//                 return !pause_requested_.load() || stop_requested_.load() || seek_requested_.load();
//             });
//         }

//         if (stop_requested_.load())
//             break;

//         if (seek_requested_.exchange(false)) {
//             current_seek_time = seek_timestamp_sec_.load();
//             int64_t seek_target = static_cast<int64_t>(current_seek_time * AV_TIME_BASE);

//             std::cout << "[Demuxer Thread] Seeking to " << current_seek_time << "s" << '\n';
//             int ret = av_seek_frame(ctx, -1, seek_target, AVSEEK_FLAG_BACKWARD);
//             if (ret < 0) {
//                 std::cerr << "[Demuxer Thread] Error seeking: " << ret << '\n';
//             } else {
//                 avformat_flush(ctx);
//                 if (packet_sink_) {
//                     Packet flush_packet = Packet::createFlushPacket();
//                     packet_sink_(flush_packet);
//                 }
//                 discarding_packets_after_seek = true;
//             }
//             continue;
//         }

//         Packet packet;
//         int ret = av_read_frame(ctx, packet.get());
//         AVStream* stream = ctx->streams[packet.streamIndex()];
//         AVRational tb = stream->time_base;
//         LOGD("Sending packet with pts=%.3f", packet.get()->pts * av_q2d(tb));

//         if (ret < 0) {
//             // EOF or error: send EOF packet and exit
//             if (packet_sink_) {
//                 Packet eof = Packet::createEofPacket();
//                 packet_sink_(eof);
//             }
//             break;
//         }

//         if (discarding_packets_after_seek) {
//             if (video_idx >= 0 && packet.streamIndex() == video_idx) {
//                 AVRational tb = ctx->streams[video_idx]->time_base;
//                 double pts_sec = packet.get()->pts * av_q2d(tb);
//                 if (pts_sec < current_seek_time) {
//                     continue; // still before seek target
//                 }
//                 // reached target, stop discarding
//                 discarding_packets_after_seek = false;
//             } else if (video_idx >= 0) {
//                 continue; // drop non-video until first video at/after target
//             }
//         }

//         if (packet_sink_) {
//             if (!packet_sink_(packet)) {
//                 stop_requested_.store(true);
//             }
//         }
//     }
//     log_thread_exit();
// }

// 在 Demuxer.cpp 中
void Demuxer::run()
{
    log_thread_entry();
    AVFormatContext* ctx = source_->get_format_context();
    if (!ctx) {
        LOGE("[Demuxer Thread] Error: AVFormatContext is null.");
        return;
    }

    while (!stop_requested_.load()) {
        // 1. 将暂停检查放在循环的开头
        {
            std::unique_lock<std::mutex> lock(mutex_);
            // 如果被要求暂停，就一直等待，直到被唤醒并且 pause_requested_ 变为 false
            cv_.wait(lock, [this] {
                return !pause_requested_.load() || stop_requested_.load();
            });
        }

        // 2. 检查是否在等待期间被要求停止
        if (stop_requested_.load()) {
            break;
        }

        // 3. 读取数据包
        Packet packet;
        int ret = av_read_frame(ctx, packet.get());

        if (ret < 0) {
            LOGI("Demuxer: End of file or error reached.");
            if (packet_sink_) {
                // 发送一个空的 packet 作为 EOF 信号
                auto eofP = Packet::createEofPacket();
                packet_sink_(eofP);
            }
            break;
        }

        // 4. 推送数据包
        if (packet_sink_) {
            // packet_sink_ 应该返回一个 bool 值，如果下游（队列）已关闭或无法接收，
            // 它应该返回 false，我们就可以停止 demuxing。
            // 你的 Decoder::run() 已经是这样做的，所以这里也需要
            if (!packet_sink_(packet)) {
                LOGI("Demuxer: Packet sink returned false. Assuming shutdown and exiting.");
                break; // 下游无法接收，停止工作
            }
        }
    }
    log_thread_exit();
}