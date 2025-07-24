#include "Mp4Parser.hpp"
#include "Decoder.hpp"
#include "DecoderContext.hpp"
#include "Demuxer.hpp"
#include "MediaSource.hpp"
#include "Mp4Parser/FrameProcessor.hpp"
#include "Packet.hpp"
#include "SemQueue.hpp"
#include <android/log.h>
#include <memory>

#define LOG_TAG "Mp4Parser"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

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

    std::atomic<PlayerState> state_ { PlayerState::Stopped };
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
        LOGI("Creating control thread...");
        control_thread_ = std::thread(&Impl::parser_loop, this);
    }

    ~Impl()
    {
        // [日志] 析构函数日志
        LOGI("Mp4Parser::Impl destructor called. Current state: %s", state_to_string(state_));
        if (state_ != PlayerState::Stopped) {
            LOGI("Requesting STOP from destructor.");
            Command cmd { CommandType::STOP };
            command_queue_->push(cmd);
        }
        parser_loop_should_exit_ = true;
        LOGI("Shutting down command queue...");
        command_queue_->shutdown();
        if (control_thread_.joinable()) {
            LOGI("Joining control thread...");
            control_thread_.join();
            LOGI("Control thread joined.");
        }
    }

    void post_command(Command cmd)
    {
        if (!parser_loop_should_exit_) {
            LOGD("Posting command: %d", static_cast<int>(cmd.type));
            command_queue_->push(cmd);
        } else {
            LOGW("Parser is shutting down. Ignoring command: %d", static_cast<int>(cmd.type));
        }
    }

    void parser_loop()
    {
        LOGI("Control thread started.");
        while (!parser_loop_should_exit_) {
            Command cmd;
            // [日志] 等待命令
            LOGD("Control thread waiting for command...");
            if (!command_queue_->wait_and_pop(cmd)) {
                // [日志] 队列被关闭，准备退出
                LOGI("Command queue was shut down. Exiting control thread loop.");
                break;
            }

            // [日志] 收到并处理命令
            LOGI("Control thread received command: %d", static_cast<int>(cmd.type));
            switch (cmd.type) {
            case CommandType::START:
                handle_start();
                break;
            case CommandType::STOP:
                handle_stop();
                break;
            case CommandType::PAUSE:
                if (state_ == PlayerState::Running)
                    handle_pause();
                else
                    LOGW("Ignoring PAUSE command, not in Running state.");
                break;
            case CommandType::RESUME:
                if (state_ == PlayerState::Paused)
                    handle_resume();
                else
                    LOGW("Ignoring RESUME command, not in Paused state.");
                break;
            case CommandType::SEEK:
                if (state_ == PlayerState::Running || state_ == PlayerState::Paused) {
                    handle_seek(cmd.time_sec);
                } else
                    LOGW("Ignoring SEEK command, not in a seekable state.");
                break;
            }
        }
        if (state_ != PlayerState::Stopped) {
            LOGW("Control loop exited, but parser state was not Stopped. Forcing cleanup...");
            handle_stop();
        }
        LOGI("Control thread finished.");
    }

private:
    void handle_start()
    {
        if (state_ != PlayerState::Stopped) {
            LOGW("START command received, but parser is not in Stopped state. Ignoring.");
            return;
        }
        LOGI("Handling START command...");

        // [日志] 管道初始化日志
        LOGI("Initializing video pipeline...");
        try {
            video_packet_queue_ = std::make_unique<SemQueue<Packet>>(config.max_packet_queue_size);
            auto video_codec_context = std::make_shared<DecoderContext>(source->get_video_codecpar());
            video_decoder_ = std::make_unique<Decoder>(video_codec_context, *video_packet_queue_);

            auto on_video_frame_cb = [this](const AVFrame* frame) {
                // [日志] 确认视频帧解码回调被触发
                LOGD("Video frame decoded callback triggered. PTS: %.3f", frame->pts * av_q2d(source->get_video_stream()->time_base));
                if (callbacks.on_video_frame_decoded) {
                    callbacks.on_video_frame_decoded(convert_video_frame(source->get_video_stream(), frame));
                }
            };
            video_decoder_->Start(on_video_frame_cb);
            LOGI("Video pipeline initialized successfully.");
        } catch (const std::exception& e) {
            LOGE("Failed to initialize video pipeline: %s. Continuing with audio only.", e.what());
            video_decoder_.reset();
            video_packet_queue_.reset();
        }

        LOGI("Initializing audio pipeline...");
        if (source->has_audio_stream()) {
            try {
                audio_packet_queue_ = std::make_unique<SemQueue<Packet>>(config.max_audio_packet_queue_size);
                auto audio_codec_context = std::make_shared<DecoderContext>(source->get_audio_codecpar());
                audio_decoder_ = std::make_unique<Decoder>(audio_codec_context, *audio_packet_queue_);

                auto on_audio_frame_cb = [this](const AVFrame* frame) {
                    // [日志] 确认音频帧解码回调被触发
                    LOGD("Audio frame decoded callback triggered. PTS: %.3f", frame->pts * av_q2d(source->get_audio_stream()->time_base));
                    if (callbacks.on_audio_frame_decoded) {
                        callbacks.on_audio_frame_decoded(convert_audio_frame(source->get_audio_stream(), frame));
                    }
                };
                audio_decoder_->Start(on_audio_frame_cb);
                LOGI("Audio pipeline initialized successfully.");
            } catch (const std::exception& e) {
                LOGE("Failed to initialize audio pipeline: %s. Continuing with video only.", e.what());
                audio_decoder_.reset();
                audio_packet_queue_.reset();
            }
        } else {
            LOGW("No audio stream found in media source.");
        }

        if (!video_decoder_ && !audio_decoder_) {
            report_error("Both video and audio pipelines failed to initialize.");
            cleanup_resources();
            return;
        }

        auto on_packet_cb = [this](Packet& packet) -> bool {
            if (state_.load() != PlayerState::Running) {
                // [日志] 如果不是Running状态，Demuxer应该停止
                LOGD("Demuxer callback: state is not Running, requesting stop.");
                return false;
            }

            // [日志] 确认数据包路由
            if (video_decoder_ && packet.streamIndex() == source->get_video_stream_index()) {
                LOGD("Routing video packet (PTS: %lld) to video queue. Queue size: %zu", packet.get()->pts, video_packet_queue_->size());
                return video_packet_queue_->push(std::move(packet));
            } else if (audio_decoder_ && packet.streamIndex() == source->get_audio_stream_index()) {
                LOGD("Routing audio packet (PTS: %lld) to audio queue. Queue size: %zu", packet.get()->pts, audio_packet_queue_->size());
                return audio_packet_queue_->push(std::move(packet));
            }

            LOGD("Dropping packet from unknown stream index: %d", packet.streamIndex());
            return true;
        };

        // [日志] 启动Demuxer
        LOGI("Starting Demuxer...");
        demuxer->Start(on_packet_cb);
        set_state(PlayerState::Running);
        LOGI("Parser started.");
    }
    void handle_stop()
    {
        if (state_ == PlayerState::Stopped)
            return;
        LOGI("Handling STOP command...");
        set_state(PlayerState::Stopped);

        // [日志] 停止各个组件
        LOGI("Stopping Demuxer...");
        if (demuxer)
            demuxer->Stop();

        LOGI("Stopping video pipeline...");
        if (video_packet_queue_)
            video_packet_queue_->shutdown();
        if (video_decoder_)
            video_decoder_->Stop();

        LOGI("Stopping audio pipeline...");
        if (audio_packet_queue_)
            audio_packet_queue_->shutdown();
        if (audio_decoder_)
            audio_decoder_->Stop();

        LOGI("Cleaning up resources...");
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
        if (demuxer) {
            demuxer->SeekTo(time_sec);
        }
        // 修改：清空所有队列和解码器
        if (video_packet_queue_)
            video_packet_queue_->clear();
        if (audio_packet_queue_)
            audio_packet_queue_->clear();

        if (video_decoder_)
            video_decoder_->flush();

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
        if (state_ == s)
            return;
        LOGI("State changed from %s to: %s", state_to_string(state_), state_to_string(s));
        state_ = s;
        if (callbacks.on_state_changed) {
            callbacks.on_state_changed(s);
        }
    }

    void report_error(const std::string& msg)
    {
        LOGE("Error reported: %s", msg.c_str());
        if (state_.load() != PlayerState::Error) {
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
    // [日志] 记录创建过程
    LOGI("Mp4Parser::create called.");
    auto instance = std::unique_ptr<Mp4Parser>(new Mp4Parser());

    try {
        LOGI("Mp4Parser::create - Initializing media source for: %s", config.file_path.c_str());
        auto source = std::make_shared<MediaSource>();
        source->open(config.file_path);

        // [日志] 检查流信息
        if (source->has_video_stream()) {
            LOGI("Video stream found. Index: %d", source->get_video_stream_index());
        } else {
            LOGW("No video stream found.");
        }
        if (source->has_audio_stream()) {
            LOGI("Audio stream found. Index: %d", source->get_audio_stream_index());
        } else {
            LOGW("No audio stream found.");
        }

        auto demuxer = std::make_unique<Demuxer>(source);
        LOGI("Mp4Parser instance skeleton created. Media source and demuxer are ready.");

        // 只有在所有同步操作都成功后，才创建 Impl 和控制线程
        instance->impl_ = std::make_unique<Impl>(config, callbacks);
        instance->impl_->source = std::move(source);
        instance->impl_->demuxer = std::move(demuxer);

    } catch (const std::exception& e) {
        LOGE("Failed to create Mp4Parser during initial setup: %s", e.what());
        return nullptr;
    }

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

player_utils::AudioParams Mp4Parser::getAudioParams() const
{
    if (impl_ && impl_->source) {
        return impl_->source->get_audio_params();
    }
    return { 0, 0 }; // 返回无效值
}
PlayerState Mp4Parser::get_state() const
{
    return impl_ ? impl_->state_.load() : PlayerState::Stopped;
}

} // namespace mp4parser