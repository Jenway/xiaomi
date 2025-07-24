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
        control_thread_ = std::thread(&Impl::parser_loop, this);
    }

    ~Impl()
    {
        if (state_ != PlayerState::Stopped) {
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
                if (state_ == PlayerState::Running)
                    handle_pause();
                break;
            case CommandType::RESUME:
                if (state_ == PlayerState::Paused)
                    handle_resume();
                break;
            case CommandType::SEEK:
                if (state_ == PlayerState::Running || state_ == PlayerState::Paused) {
                    handle_seek(cmd.time_sec);
                }
                break;
            }
        }
        if (state_ != PlayerState::Stopped) {
            handle_stop();
        }
        LOGI("Control thread finished.");
    }

private:
    // --- 命令处理器 (在控制线程中安全执行) ---
    void handle_start()
    {
        if (state_ != PlayerState::Stopped)
            return;
        LOGI("Handling START command...");
        // 2. 独立初始化视频管道
        try {
            video_packet_queue_ = std::make_unique<SemQueue<Packet>>(config.max_packet_queue_size);
            auto video_codec_context = std::make_shared<DecoderContext>(source->get_video_codecpar());
            video_decoder_ = std::make_unique<Decoder>(video_codec_context, *video_packet_queue_);

            auto on_video_frame_cb = [this](const AVFrame* frame) {
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

        // 3. 独立初始化音频管道
        if (source->has_audio_stream()) {
            try {
                audio_packet_queue_ = std::make_unique<SemQueue<Packet>>(config.max_audio_packet_queue_size);
                auto audio_codec_context = std::make_shared<DecoderContext>(source->get_audio_codecpar());
                audio_decoder_ = std::make_unique<Decoder>(audio_codec_context, *audio_packet_queue_);

                auto on_audio_frame_cb = [this](const AVFrame* frame) {
                    if (callbacks.on_audio_frame_decoded) {
                        /*
                        LOGI("Decoded Audio Frame: nb_samples=%d, sample_rate=%d, channels=%d, format=%s",
                            frame->nb_samples, frame->sample_rate, frame->ch_layout.nb_channels,
                            av_get_sample_fmt_name((AVSampleFormat)frame->format));

                        // Also check if it's planar or interleaved
                        LOGI("  Is Planar: %s", av_sample_fmt_is_planar((AVSampleFormat)frame->format) ? "true" : "false");
                        for (int i = 0; i < AV_NUM_DATA_POINTERS && frame->data[i] != nullptr; ++i) {
                            LOGI("  data[%d]=%p, linesize[%d]=%d", i, frame->data[i], i, frame->linesize[i]);
                        }
                        */
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
        }

        // 4. 检查是否所有管道都初始化失败
        if (!video_decoder_ && !audio_decoder_) {
            report_error("Both video and audio pipelines failed to initialize.");
            cleanup_resources();
            return;
        }

        // 5. 在所有依赖项都创建后，再定义解复用回调
        auto on_packet_cb = [this](Packet& packet) -> bool {
            if (state_.load() == PlayerState::Stopped || state_.load() == PlayerState::Error) {
                return false; // 指示Demuxer停止
            }

            // 路由：根据流索引将包放入正确的队列
            if (video_decoder_ && packet.streamIndex() == source->get_video_stream_index()) {
                return video_packet_queue_->push(std::move(packet));
            } else if (audio_decoder_ && packet.streamIndex() == source->get_audio_stream_index()) {
                return audio_packet_queue_->push(std::move(packet));
            }

            return true; // 丢弃其他包，但继续解复用
        };

        // 6. 启动解复用
        demuxer->Start(on_packet_cb);
        set_state(PlayerState::Running);
        LOGI("Parser started.");
    }

    void handle_stop()
    {
        if (state_ == PlayerState::Stopped)
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
    // 使用 shared_ptr 来管理实例，因为 Impl 内部需要一个 this 指针
    auto instance = std::unique_ptr<Mp4Parser>(new Mp4Parser());
    instance->impl_ = std::make_unique<Impl>(config, callbacks);

    // --- 新增的同步初始化逻辑 ---
    try {
        LOGI("Mp4Parser::create - Initializing media source...");
        instance->impl_->source = std::make_shared<MediaSource>();
        instance->impl_->source->open(config.file_path); // 同步打开文件并解析元数据
        instance->impl_->demuxer = std::make_unique<Demuxer>(instance->impl_->source);
        LOGI("Mp4Parser instance created and media source initialized.");
    } catch (const std::exception& e) {
        LOGE("Failed to create Mp4Parser: %s", e.what());
        // 如果初始化失败，返回 nullptr
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