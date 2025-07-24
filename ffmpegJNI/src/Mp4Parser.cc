#include "Mp4Parser.hpp"
#include "Decoder.hpp"
#include "DecoderContext.hpp"
#include "Demuxer.hpp"
#include "MediaSource.hpp"
#include "Mp4Parser/FrameProcessor.hpp"
#include "Packet.hpp"
#include "SemQueue.hpp"
#include <android/log.h>

#define LOG_TAG "Mp4Parser"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace mp4parser {

// 命令结构体，用于在API线程和控制线程之间通信
enum class CommandType { START,
    STOP,
    PAUSE,
    RESUME,
    SEEK };
struct Command {
    CommandType type;
    double time_sec = 0.0; // 仅用于 SEEK
};

using ffmpeg_utils::Packet;
using player_utils::SemQueue;

struct Mp4Parser::Impl {
    Config config;
    Callbacks callbacks;

    std::atomic<PlayerState> state { PlayerState::Stopped };
    std::thread control_thread_;
    std::unique_ptr<SemQueue<Command>> command_queue_;
    std::atomic<bool> parser_loop_should_exit_ { false };

    // --- Core Components ---
    std::shared_ptr<MediaSource> source;
    std::unique_ptr<Demuxer> demuxer;
    std::unique_ptr<SemQueue<Packet>> packet_queue;
    std::unique_ptr<Decoder> decoder;

    Impl(Config cfg, Callbacks cbs)
        : config(std::move(cfg))
        , callbacks(std::move(cbs))
    {
        command_queue_ = std::make_unique<SemQueue<Command>>(16); // 命令队列不需要很大
        control_thread_ = std::thread(&Impl::parser_loop, this);
    }

    ~Impl()
    {
        if (state != PlayerState::Stopped) {
            // 发送停止命令并等待控制线程结束
            Command cmd { CommandType::STOP };
            command_queue_->push(cmd);
        }
        parser_loop_should_exit_ = true;
        command_queue_->shutdown(); // 确保即使在等待命令时也能唤醒控制线程
        if (control_thread_.joinable()) {
            control_thread_.join();
        }
    }

    // --- 命令分发器 ---
    void post_command(Command cmd)
    {
        if (!parser_loop_should_exit_) {
            command_queue_->push(cmd);
        }
    }

    void parser_loop()
    {
        LOGI("Control thread started.");
        while (!parser_loop_should_exit_) {
            Command cmd;
            if (!command_queue_->wait_and_pop(cmd)) {
                break;
            }

            switch (cmd.type) {
            case CommandType::START:
                handle_start();
                break;
            case CommandType::STOP:
                handle_stop();
                break;
            case CommandType::PAUSE:
                if (state == PlayerState::Running)
                    handle_pause();
                break;
            case CommandType::RESUME:
                if (state == PlayerState::Paused)
                    handle_resume();
                break;
            case CommandType::SEEK:
                if (state == PlayerState::Running || state == PlayerState::Paused) {
                    handle_seek(cmd.time_sec);
                }
                break;
            }
        }
        if (state != PlayerState::Stopped) {
            handle_stop();
        }
        LOGI("Control thread finished.");
    }

private:
    // --- 命令处理器 (在控制线程中安全执行) ---
    void handle_start()
    {
        if (state != PlayerState::Stopped)
            return;
        LOGI("Handling START command...");
        try {
            source = std::make_shared<MediaSource>();
            source->open(config.file_path);
            demuxer = std::make_unique<Demuxer>(source);
            packet_queue = std::make_unique<SemQueue<Packet>>(config.max_packet_queue_size);
            auto codec_context = std::make_shared<DecoderContext>(source->get_video_codecpar());
            decoder = std::make_unique<Decoder>(codec_context, *packet_queue);

            auto on_packet_cb = [this](Packet& packet) -> bool {
                if (state == PlayerState::Stopped || state == PlayerState::Error)
                    return false;
                return packet_queue->push(std::move(packet));
            };
            auto on_frame_cb = [this](const AVFrame* frame) {
                if (state == PlayerState::Stopped || state == PlayerState::Error)
                    return;
                if (callbacks.on_frame_decoded) {
                    callbacks.on_frame_decoded(convert_frame(source->get_video_stream(), frame));
                }
            };

            decoder->Start(on_frame_cb);
            demuxer->Start(on_packet_cb);
            set_state(PlayerState::Running);
            LOGI("Parser started successfully.");
        } catch (const std::exception& e) {
            report_error(e.what());
            cleanup_resources();
            set_state(PlayerState::Error);
        }
    }

    void handle_stop()
    {
        if (state == PlayerState::Stopped)
            return;
        LOGI("Handling STOP command...");
        set_state(PlayerState::Stopped); // 立即改变状态以阻止回调

        // 按照数据流逆序关闭
        if (demuxer)
            demuxer->Stop();
        if (packet_queue)
            packet_queue->shutdown();
        if (decoder)
            decoder->Stop();

        cleanup_resources();
        LOGI("Parser stopped successfully.");
    }

    void handle_pause()
    {
        LOGI("Handling PAUSE command...");
        demuxer->Pause();
        set_state(PlayerState::Paused);
    }

    void handle_resume()
    {
        LOGI("Handling RESUME command...");
        demuxer->Resume();
        set_state(PlayerState::Running);
    }

    void handle_seek(double time_sec) const
    {
        LOGI("Handling SEEK to %.3f sec...", time_sec);
        demuxer->SeekTo(time_sec);
        if (packet_queue)
            packet_queue->clear();
        if (decoder)
            decoder->flush();
        LOGI("Seek command processed.");
    }

    void cleanup_resources()
    {
        decoder.reset();
        packet_queue.reset();
        demuxer.reset();
        source.reset();
        LOGI("Core components cleaned up.");
    }

    void set_state(PlayerState s)
    {
        if (state == s)
            return;
        LOGI("State changed from %s to: %s", state_to_string(state), state_to_string(s));
        state = s;
        if (callbacks.on_state_changed) {
            callbacks.on_state_changed(s);
        }
    }

    void report_error(const std::string& msg)
    {
        LOGE("Error reported: %s", msg.c_str());
        if (state.load() != PlayerState::Error) {
            set_state(PlayerState::Error);
            if (callbacks.on_error) {
                callbacks.on_error(msg);
            }
        }
    }
};

// --- Public API ---

std::unique_ptr<Mp4Parser> Mp4Parser::create(const Config& config, const Callbacks& callbacks)
{
    auto instance = std::unique_ptr<Mp4Parser>(new Mp4Parser());
    instance->impl_ = std::make_unique<Impl>(config, callbacks);
    LOGI("Mp4Parser instance created.");
    return instance;
}
Mp4Parser::~Mp4Parser()
{
    LOGI("Mp4Parser instance Deleted.");
}

void Mp4Parser::start()
{
    if (impl_)
        impl_->post_command({ CommandType::START });
}

void Mp4Parser::pause()
{
    if (impl_)
        impl_->post_command({ CommandType::PAUSE });
}

void Mp4Parser::resume()
{
    if (impl_)
        impl_->post_command({ CommandType::RESUME });
}

void Mp4Parser::stop()
{
    if (impl_)
        impl_->post_command({ CommandType::STOP });
}

void Mp4Parser::seek(double time_sec)
{
    if (impl_)
        impl_->post_command({ CommandType::SEEK, time_sec });
}

double Mp4Parser::get_duration()
{
    if (impl_) {
        return impl_->demuxer->GetDuration();
    }
}
PlayerState Mp4Parser::get_state() const
{
    return impl_ ? impl_->state.load() : PlayerState::Stopped;
}

} // namespace mp4parser