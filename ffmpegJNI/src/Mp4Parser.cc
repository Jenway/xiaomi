#include "Mp4Parser.hpp"
#include "BinarySemaphore.hpp"
#include "Decoder.hpp"
#include "DecoderContext.hpp"
#include "Demuxer.hpp"
#include "MediaSource.hpp"
#include "Mp4Parser/FrameProcessor.hpp"
#include "SemQueue.hpp"

#include <android/log.h>
#include <atomic>
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

// Define logging macros for convenience
#define LOG_TAG "Mp4Parser"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

using ffmpeg_utils::Packet;
using player_utils::SemQueue;

namespace mp4parser {

struct Mp4Parser::Impl {
    Config config;
    Callbacks callbacks;

    std::atomic<PlayerState> state { PlayerState::Stopped };
    std::thread demuxer_thread;
    std::thread decoder_thread;

    // --- Synchronization ---
    std::mutex control_mutex_;
    player_utils::BinarySemaphore run_gate_ { true }; // 初始为true, 表示门是开的 (可以运行)
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

            source = std::make_unique<MediaSource>(config.file_path);
            packet_queue = std::make_unique<SemQueue<Packet>>(config.max_packet_queue_size);

            // The callback passed to the decoder, which will push frames out
            auto on_frame_decoded_cb = [this](const AVFrame* frame) {
                // 1. 等待门打开
                run_gate_.acquire();

                // 2. 检查是否要退出
                if (stop_requested_) {
                    // 如果要退出，不用把门再打开了，因为没人需要通过了
                    return;
                }

                // 3. **关键**：立即把门再打开，以便下一次循环可以再次检查
                run_gate_.release();

                // 4. 在门外执行耗时操作
                if (callbacks.on_frame_decoded) {
                    callbacks.on_frame_decoded(convert_frame(source->get_video_stream(), frame));
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
        run_gate_.acquire();
        set_state(PlayerState::Paused);
    }

    void do_resume()
    {
        if (state != PlayerState::Paused)
            return;
        LOGI("Resuming...");
        run_gate_.release();
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

        if (packet_queue) {
            packet_queue->shutdown();
        }
        run_gate_.release(); // 打开门，确保正在暂停等待的解码线程能通过并退出
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
    if (!impl_)
        return;

    // get Handler
    std::thread demuxer_to_join;
    std::thread decoder_to_join;

    {
        std::lock_guard<std::mutex> lock(impl_->control_mutex_);
        if (impl_->state.load() == PlayerState::Stopped) {
            return;
        }
        impl_->do_stop();

        // get thread Handler
        if (impl_->demuxer_thread.joinable()) {
            demuxer_to_join.swap(impl_->demuxer_thread);
        }
        if (impl_->decoder_thread.joinable()) {
            decoder_to_join.swap(impl_->decoder_thread);
        }
    }
    // Real join
    LOGI("Joining demuxer thread...");
    if (demuxer_to_join.joinable()) {
        demuxer_to_join.join();
    }
    LOGI("Joining decoder thread...");
    if (decoder_to_join.joinable()) {
        decoder_to_join.join();
    }

    {
        std::lock_guard<std::mutex> load(impl_->control_mutex_);
        impl_->decoder.reset();
        impl_->packet_queue.reset();
        impl_->source.reset();
        impl_->set_state(PlayerState::Stopped);
        LOGI("MP4 Parser Stopped successfully");
    }
}

// Add the public seek method
void Mp4Parser::seek(double time_sec)
{
    std::lock_guard<std::mutex> lock(impl_->control_mutex_);
    if (impl_) {
        auto current_state = impl_->state.load();
        if (current_state != PlayerState::Running && current_state != PlayerState::Paused) {
            LOGE("Cannot seek in current state: %s", state_to_string(current_state));
            return;
        }
        impl_->do_seek(time_sec);
    }
}

PlayerState Mp4Parser::get_state() const
{
    return impl_ ? impl_->state.load() : PlayerState::Stopped;
}

} // namespace mp4parser