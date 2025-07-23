#include "Mp4Parser.hpp" // Header should be first
#include "DecoderContext.hpp"

#include <android/log.h>
#include <atomic>
#include <cinttypes> // For PRId64 format specifier
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>

// FFmpeg headers
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
}

// Your project's internal headers
#include "Decoder.hpp"
#include "Demuxer.hpp"
#include "MediaSource.hpp"
#include "SemQueue.hpp"

// Define logging macros for convenience
#define LOG_TAG "Mp4Parser"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using ffmpeg_utils::Packet;
using player_utils::SemQueue;

namespace mp4parser {

// Helper function for readable log output
static const char* state_to_string(PlayerState state)
{
    switch (state) {
    case PlayerState::Stopped:
        return "Stopped";
    case PlayerState::Running:
        return "Running";
    case PlayerState::Paused:
        return "Paused";
    case PlayerState::Finished:
        return "Finished";
    case PlayerState::Error:
        return "Error";
    default:
        return "Unknown";
    }
}

struct Mp4Parser::Impl {
    Config config;
    Callbacks callbacks;

    std::atomic<PlayerState> state { PlayerState::Stopped };
    std::thread demuxer_thread;
    std::thread decoder_thread;

    // --- Synchronization ---
    std::mutex control_mutex_; // Protects all public control methods
    std::mutex pause_mutex_;
    std::condition_variable pause_cv_;
    std::atomic<bool> paused_ { false };
    std::atomic<bool> stop_requested_ { false };

    // --- Core Components ---
    std::unique_ptr<MediaSource> source;
    std::unique_ptr<SemQueue<Packet>> packet_queue;
    std::unique_ptr<Decoder> decoder;

    Impl(Config cfg, Callbacks cbs)
        : config(std::move(cfg))
        , callbacks(std::move(cbs))
    {
    }

    ~Impl()
    {
        // Ensure stop is called on destruction
        if (state != PlayerState::Stopped) {
            do_stop();
        }
    }

    // --- Main Logic ---
    void run()
    {
        std::lock_guard<std::mutex> lock(control_mutex_);
        if (state != PlayerState::Stopped) {
            LOGE("Parser is already running or in an invalid state.");
            return;
        }
        LOGI("Starting parser...");

        try {
            stop_requested_ = false;
            paused_ = false;

            source = std::make_unique<MediaSource>(config.file_path);
            packet_queue = std::make_unique<SemQueue<Packet>>(config.max_packet_queue_size);

            // The callback passed to the decoder, which will push frames out
            auto on_frame_decoded_cb = [this](const AVFrame* frame) {
                // Handle pause logic
                std::unique_lock<std::mutex> lock(pause_mutex_);
                pause_cv_.wait(lock, [this] { return !paused_ || stop_requested_; });
                if (stop_requested_)
                    return;

                // Convert and submit the frame
                if (callbacks.on_frame_decoded) {
                    callbacks.on_frame_decoded(convert_frame(frame));
                }
            };
            auto codec_context = std::make_shared<DecoderContext>(source->get_video_codecpar());
            decoder = std::make_unique<Decoder>(codec_context, *packet_queue, on_frame_decoded_cb);

            LOGI("Source and decoder initialized successfully.");
            set_state(PlayerState::Running);

            demuxer_thread = std::thread(&Impl::demuxer_loop, this);
            decoder_thread = std::thread(&Impl::decoder_loop, this);

        } catch (const std::exception& e) {
            report_error(e.what());
        }
    }

    void demuxer_loop()
    {
        LOGI("Demuxer thread started.");
        try {
            player_utils::demuxer(source->get_format_context(), *packet_queue, source->get_video_stream_index());
        } catch (const std::exception& e) {
            report_error(e.what());
        }
        packet_queue->shutdown();
        LOGI("Demuxer thread finished.");
    }

    void decoder_loop()
    {
        LOGI("Decoder thread started.");
        try {
            decoder->run();
            // If the decoder finishes without being stopped, the playback is complete
            if (!stop_requested_) {
                set_state(PlayerState::Finished);
            }
        } catch (const std::exception& e) {
            report_error(e.what());
        }
        LOGI("Decoder thread finished.");
    }

    // --- Control Methods ---
    void do_pause()
    {
        if (state != PlayerState::Running)
            return;
        LOGI("Pausing...");
        paused_ = true;
        set_state(PlayerState::Paused);
    }

    void do_resume()
    {
        if (state != PlayerState::Paused)
            return;
        LOGI("Resuming...");
        std::unique_lock<std::mutex> lock(pause_mutex_);
        paused_ = false;
        pause_cv_.notify_all();
        set_state(PlayerState::Running);
    }

    void do_stop()
    {
        LOGI("Stopping parser...");
        if (state == PlayerState::Stopped) {
            LOGI("Already stopped.");
            return;
        }

        stop_requested_ = true;

        // Unblock any waiting threads
        if (packet_queue)
            packet_queue->shutdown();
        {
            std::unique_lock<std::mutex> lock(pause_mutex_);
            pause_cv_.notify_all();
        }

        if (demuxer_thread.joinable())
            demuxer_thread.join();
        if (decoder_thread.joinable())
            decoder_thread.join();

        // Reset components
        decoder.reset();
        packet_queue.reset();
        source.reset();

        LOGI("Parser stopped successfully.");
        set_state(PlayerState::Stopped);
    }

    void do_seek(double time_sec)
    {
        if (!source || state == PlayerState::Stopped || state == PlayerState::Error) {
            LOGE("Cannot seek in current state: %s", state_to_string(state));
            return;
        }
        LOGI("Seek requested to %.3f seconds.", time_sec);

        // 1. Calculate target timestamp in stream's time_base
        AVStream* stream = source->get_video_stream();
        double target_ts = time_sec / av_q2d(stream->time_base);

        // 2. Perform the seek on the format context
        // AVSEEK_FLAG_BACKWARD ensures we seek to a keyframe at or before the target time
        if (av_seek_frame(source->get_format_context(), source->get_video_stream_index(), static_cast<int64_t>(target_ts), AVSEEK_FLAG_BACKWARD) < 0) {
            report_error("Failed to seek frame.");
            return;
        }

        // 3. Flush decoder and packet queue
        // This is crucial to discard all old data
        decoder->flush();
        packet_queue->clear();

        LOGI("Seek completed. Decoder and queue flushed.");
        // Note: No need to pause/resume threads, they will naturally continue
        // from the new position once the queue is populated again.
    }

    // --- Helpers ---
    std::shared_ptr<VideoFrame> convert_frame(const AVFrame* frame) const
    {
        auto out = std::make_shared<VideoFrame>();
        out->width = frame->width;
        out->height = frame->height;
        out->format = static_cast<AVPixelFormat>(frame->format);

        // 1. 正确转换PTS (显示时间戳)
        if (frame->pts != AV_NOPTS_VALUE) {
            AVStream* stream = source->get_video_stream();
            out->pts = static_cast<double>(frame->pts) * av_q2d(stream->time_base);
            LOGD("Converting AVFrame: Format=%s, Size=%dx%d, PTS=%.3f",
                av_get_pix_fmt_name(static_cast<AVPixelFormat>(frame->format)),
                frame->width, frame->height, out->pts);
        } else {
            out->pts = 0.0;
            LOGD("Converted frame. No PTS value found.");
        }

        // --- 拷贝图像数据 ---

        if (!frame->data[0]) {
            LOGE("AVFrame data[0] is null, cannot convert frame.");
            return nullptr;
        }

        // 2. 计算所有平面需要的总字节数
        int total_size = 0;
        // 通常对于 YUV420P, 只有 data[0], data[1], data[2] 是有效的
        for (int i = 0; i < AV_NUM_DATA_POINTERS && (frame->data[i] != nullptr); ++i) {
            // Y 平面的高度是 frame->height，U和V平面的高度是 frame->height / 2
            int plane_height = (i == 0) ? frame->height : (frame->height + 1) / 2;
            total_size += frame->linesize[i] * plane_height;
        }

        if (total_size <= 0) {
            LOGE("Calculated total frame size is %d, which is invalid.", total_size);
            return nullptr;
        }

        // 3. 为我们自己的 VideoFrame 分配内存
        out->data.resize(total_size);
        uint8_t* dst = out->data.data();

        // 4. 逐个平面地拷贝数据，并记录 linesize
        for (int i = 0; i < AV_NUM_DATA_POINTERS && (frame->data[i] != nullptr); ++i) {
            int plane_height = (i == 0) ? frame->height : (frame->height + 1) / 2;
            int bytes_to_copy = frame->linesize[i] * plane_height;

            // 拷贝整个平面
            memcpy(dst, frame->data[i], bytes_to_copy);

            // 更新目标指针
            dst += bytes_to_copy;

            // 记录这一平面的 linesize，渲染器会用到它
            out->linesize[i] = frame->linesize[i];
        }

        LOGD("Frame data copied, total size: %d bytes.", total_size);

        return out;
    }

    void set_state(PlayerState s)
    {
        LOGI("State changed to: %s", state_to_string(s));
        state = s;
        if (callbacks.on_state_changed) {
            callbacks.on_state_changed(s);
        }
    }

    void report_error(const std::string& msg)
    {
        LOGE("Error reported: %s", msg.c_str());
        if (state.load() != PlayerState::Error) { // Report error only once
            state = PlayerState::Error;
            if (callbacks.on_error) {
                callbacks.on_error(msg);
            }
        }
    }
};

// --- Public API Implementation ---

std::unique_ptr<Mp4Parser> Mp4Parser::create(const Config& config, const Callbacks& callbacks)
{
    auto instance = std::unique_ptr<Mp4Parser>(new Mp4Parser());
    instance->impl_ = std::make_unique<Impl>(config, callbacks);
    LOGI("Mp4Parser instance created.");
    return instance;
}

Mp4Parser::~Mp4Parser()
{
    LOGI("Destroying Mp4Parser instance.");
    if (impl_) {
        // The ~Impl() destructor will handle the stop call.
        impl_.reset();
    }
}

void Mp4Parser::start()
{
    if (impl_)
        impl_->run();
}

void Mp4Parser::pause()
{
    std::lock_guard<std::mutex> lock(impl_->control_mutex_);
    if (impl_)
        impl_->do_pause();
}

void Mp4Parser::resume()
{
    std::lock_guard<std::mutex> lock(impl_->control_mutex_);
    if (impl_)
        impl_->do_resume();
}

void Mp4Parser::stop()
{
    std::lock_guard<std::mutex> lock(impl_->control_mutex_);
    if (impl_)
        impl_->do_stop();
}

// Add the public seek method
void Mp4Parser::seek(double time_sec)
{
    std::lock_guard<std::mutex> lock(impl_->control_mutex_);
    if (impl_)
        impl_->do_seek(time_sec);
}

PlayerState Mp4Parser::get_state() const
{
    return impl_ ? impl_->state.load() : PlayerState::Stopped;
}

} // namespace mp4parser