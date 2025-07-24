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

    // 视频处理管道
    std::unique_ptr<SemQueue<Packet>> video_packet_queue_;
    std::unique_ptr<Decoder> video_decoder_;

    // 音频处理管道
    std::unique_ptr<SemQueue<Packet>> audio_packet_queue_;
    std::unique_ptr<Decoder> audio_decoder_;

    Impl(Config cfg, Callbacks cbs)
        : config(std::move(cfg))
        , callbacks(std::move(cbs))
    {
        command_queue_ = std::make_unique<SemQueue<Command>>(16);
        control_thread_ = std::thread(&Impl::parser_loop, this);
    }

    ~Impl()
    {
        if (state != PlayerState::Stopped) {
            Command cmd { CommandType::STOP };
            command_queue_->push(cmd);
        }
        parser_loop_should_exit_ = true;
        command_queue_->shutdown();
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
            // Video
            video_packet_queue_ = std::make_unique<SemQueue<Packet>>(config.max_packet_queue_size);
            auto video_codec_context = std::make_shared<DecoderContext>(source->get_video_codecpar());
            video_decoder_ = std::make_unique<Decoder>(video_codec_context, *video_packet_queue_);
            // Audio
            if (source->has_audio_stream()) {
                audio_packet_queue_ = std::make_unique<SemQueue<Packet>>(config.max_audio_packet_queue_size); // 可能需要新的配置项
                auto audio_codec_context = std::make_shared<DecoderContext>(source->get_audio_codecpar());
                audio_decoder_ = std::make_unique<Decoder>(audio_codec_context, *audio_packet_queue_);
            }

            auto on_packet_cb = [this](Packet& packet) -> bool {
                if (state == PlayerState::Stopped || state == PlayerState::Error)
                    return false;

                if (packet.streamIndex() == source->get_video_stream_index()) {
                    return video_packet_queue_->push(std::move(packet));
                } else if (source->has_audio_stream() && packet.streamIndex() == source->get_audio_stream_index()) {
                    return audio_packet_queue_->push(std::move(packet));
                }
                // 非音视频包，直接丢弃
                return true;
            };

            // --- 5. 修改：为每个Decoder设置独立的回调 ---
            auto on_video_frame_cb = [this](const AVFrame* frame) {
                if (callbacks.on_video_frame_decoded) {
                    // 假设 convert_video_frame 存在
                    callbacks.on_video_frame_decoded(convert_video_frame(source->get_video_stream(), frame));
                }
            };

            video_decoder_->Start(on_video_frame_cb);

            // 新增：启动音频解码器
            if (audio_decoder_) {
                auto on_audio_frame_cb = [this](const AVFrame* frame) {
                    if (callbacks.on_audio_frame_decoded) {
                        // 假设 convert_audio_frame 存在
                        callbacks.on_audio_frame_decoded(convert_audio_frame(source->get_audio_stream(), frame));
                    }
                };
                audio_decoder_->Start(on_audio_frame_cb);
            }

            // --- 启动解复用 ---
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
        LOGI("Mp4Parser::stop() called. Requesting shutdown.");
        set_state(PlayerState::Stopped);

        // 按照数据流逆序关闭
        if (demuxer)
            demuxer->Stop();

        // 关闭视频管道
        if (video_packet_queue_)
            video_packet_queue_->shutdown();
        if (video_decoder_)
            video_decoder_->Stop();

        // 新增：关闭音频管道
        if (audio_packet_queue_)
            audio_packet_queue_->shutdown();
        if (audio_decoder_)
            audio_decoder_->Stop();

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

        // 修改：清空所有队列和解码器
        if (video_packet_queue_)
            video_packet_queue_->clear();
        if (video_decoder_)
            video_decoder_->flush();

        if (audio_packet_queue_)
            audio_packet_queue_->clear();
        if (audio_decoder_)
            audio_decoder_->flush();

        LOGI("Seek command processed.");
    }

    void cleanup_resources()
    {
        // 修改：清理所有组件
        video_decoder_.reset();
        video_packet_queue_.reset();

        audio_decoder_.reset();
        audio_packet_queue_.reset();

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
    return -1.0;
}

PlayerState Mp4Parser::get_state() const
{
    return impl_ ? impl_->state.load() : PlayerState::Stopped;
}

} // namespace mp4parser