#include "NativePlayer.hpp"
#include "AAudioRender.h"
#include "Entitys.hpp"
#include "GLESRender.hpp"
#include "GLRenderHost.hpp"
#include "Mp4Parser.hpp"
#include "SemQueue.hpp"
#include <aaudio/AAudio.h>
#include <android/log.h>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <optional>
#include <queue>
#include <thread>
#include <utility>
#include <variant>

#define LOG_TAG "NativePlayerFSM"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)

using mp4parser::Mp4Parser;
using player_utils::AudioFrame;
using player_utils::PlayerState;
using player_utils::SemQueue;
using player_utils::VideoFrame;
using render_utils::GLRenderHost;
using std::shared_ptr;
using std::unique_ptr;

struct CommandPlay {
    std::string path;
    ANativeWindow* window {};
};
struct CommandPause {
    bool is_paused;
};
struct CommandStop { };
struct CommandSeek {
    double position;
};
struct CommandShutdown { };

using Command = std::variant<
    CommandPlay,
    CommandPause,
    CommandStop,
    CommandSeek,
    CommandShutdown>;

struct NativePlayer::Impl {
    explicit Impl(NativePlayer* self);
    ~Impl();

    void fsm_loop();

    // --- 状态和线程 ---
    NativePlayer* self_;
    std::atomic<PlayerState> state_ { PlayerState::None };
    std::thread fsm_thread_;
    std::queue<Command> command_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cond_;
    bool shutdown_requested_ = false;

    // --- core 组件 ---
    unique_ptr<Mp4Parser> parser_;
    unique_ptr<GLRenderHost> renderHost_;
    unique_ptr<AAudioRender> audio_render_;

    // Queue
    unique_ptr<SemQueue<shared_ptr<VideoFrame>>> video_frame_queue_;
    unique_ptr<SemQueue<shared_ptr<AudioFrame>>> audio_frame_queue_;

    // 主时钟，由音频播放驱动
    std::atomic<double> master_clock_pts_ { 0.0 };

    // --- 回调 ---
    static int audio_data_callback(AAudioStream* stream, void* userData, void* audioData, int32_t numFrames);
    std::function<void(PlayerState)> on_state_changed_cb_;
    std::function<void(const std::string&)> on_error_cb_;

    // 用于音频数据回调的中间缓冲区
    std::shared_ptr<AudioFrame> current_audio_frame_;
    uint8_t* audio_buffer_ptr_ = nullptr;
    int audio_buffer_size_ = 0;

private:
    void handle_play(const CommandPlay& cmd);
    void handle_pause(const CommandPause& cmd);
    void handle_stop();
    void handle_seek(const CommandSeek& cmd);
    void cleanup_resources();
    void run_sync_cycle();
};

// --- Public API (Dispatch) ---

NativePlayer::NativePlayer()
    : impl_(std::make_unique<Impl>(this))
{
}

NativePlayer::~NativePlayer()
{
    LOGI("NativePlayer destructor called.");
}

void NativePlayer::play(const std::string& path, ANativeWindow* window)
{
    LOGI("Dispatching PLAY command.");
    {
        std::lock_guard lock(impl_->queue_mutex_);
        impl_->command_queue_.push(CommandPlay { path, window });
    }
    impl_->queue_cond_.notify_one();
}

void NativePlayer::pause(bool is_paused)
{
    LOGI("Dispatching PAUSE command.");
    {
        std::lock_guard lock(impl_->queue_mutex_);
        impl_->command_queue_.push(CommandPause { is_paused });
    }
    impl_->queue_cond_.notify_one();
}

void NativePlayer::stop()
{
    LOGI("Dispatching STOP command.");
    {
        std::lock_guard lock(impl_->queue_mutex_);
        impl_->command_queue_.push(CommandStop {});
    }
    impl_->queue_cond_.notify_one();
}

void NativePlayer::seek(double time_sec)
{
    LOGI("Dispatching SEEK command.");
    {
        std::lock_guard lock(impl_->queue_mutex_);
        impl_->command_queue_.push(CommandSeek { time_sec });
    }
    impl_->queue_cond_.notify_one();
}

void NativePlayer::setOnStateChangedCallback(std::function<void(PlayerState)> cb)
{
    impl_->on_state_changed_cb_ = std::move(cb);
}
void NativePlayer::setOnErrorCallback(std::function<void(const std::string&)> cb)
{
    impl_->on_error_cb_ = std::move(cb);
}

// --- impl ---

NativePlayer::Impl::Impl(NativePlayer* self)
    : self_(self)
{
    fsm_thread_ = std::thread(&Impl::fsm_loop, this);
}

NativePlayer::Impl::~Impl()
{
    {
        std::lock_guard lock(queue_mutex_);
        shutdown_requested_ = true;
        command_queue_.emplace(CommandShutdown {});
    }

    queue_cond_.notify_one();

    if (fsm_thread_.joinable()) {
        fsm_thread_.join();
    }
}

void NativePlayer::Impl::fsm_loop()
{
    LOGI("FSM thread started.");
    while (!shutdown_requested_) {
        // --- 1. 等待事件 ---
        std::unique_lock lock(queue_mutex_);
        if (state_.load() == PlayerState::Playing) {
            // 播放时，以10ms为超时进行等待。超时后可以执行一轮同步逻辑。
            queue_cond_.wait_for(lock, std::chrono::milliseconds(10), [this] { return !command_queue_.empty(); });
        } else {
            // 其他状态下，无限等待命令。
            queue_cond_.wait(lock, [this] { return !command_queue_.empty(); });
        }

        // --- 2. 优先处理命令队列 ---
        while (!command_queue_.empty()) {
            Command cmd = std::move(command_queue_.front());
            command_queue_.pop();
            lock.unlock(); // 处理命令时解锁

            if (std::holds_alternative<CommandShutdown>(cmd)) {
                LOGI("FSM received SHUTDOWN command. Exiting loop.");
                cleanup_resources();
                shutdown_requested_ = true; // 确保外层循环退出
                break; // 退出命令处理循环
            }

            // 根据状态处理命令 (省略了 Playing 状态，因为它由同步循环处理)
            switch (state_.load()) {
            case PlayerState::None:
            case PlayerState::End:
                if (std::holds_alternative<CommandPlay>(cmd)) {
                    handle_play(std::get<CommandPlay>(cmd));
                }
                break;
            case PlayerState::Playing:
            case PlayerState::Paused:
                if (std::holds_alternative<CommandPause>(cmd)) {
                    handle_pause(std::get<CommandPause>(cmd));
                } else if (std::holds_alternative<CommandStop>(cmd)) {
                    handle_stop();
                } else if (std::holds_alternative<CommandSeek>(cmd)) {
                    handle_seek(std::get<CommandSeek>(cmd));
                }
                break;
            default:
                LOGW("Command received in unhandled state: %d", static_cast<int>(state_.load()));
                break;
            }
            lock.lock();
        }

        if (shutdown_requested_)
            break;

        // --- 3. 如果处于播放状态，则执行音视频同步 ---
        if (state_.load() == PlayerState::Playing) {
            lock.unlock();
            run_sync_cycle();
        }
    }
    LOGI("FSM thread finished.");
}

// --- 实现内部处理函数 ---

int NativePlayer::Impl::audio_data_callback(AAudioStream* stream, void* userData, void* audioData, int32_t numFrames)
{
    auto* impl = static_cast<NativePlayer::Impl*>(userData);
    if ((impl == nullptr) || !impl->audio_frame_queue_) {
        return AAUDIO_CALLBACK_RESULT_STOP;
    }

    int32_t channelCount = AAudioStream_getChannelCount(stream);
    int32_t format = AAudioStream_getFormat(stream);
    int32_t bytesPerSample = (format == AAUDIO_FORMAT_PCM_I16) ? sizeof(int16_t) : sizeof(float);
    int32_t bytesNeeded = numFrames * channelCount * bytesPerSample;
    int32_t bytesCopied = 0;

    while (bytesCopied < bytesNeeded) {
        if (impl->audio_buffer_ptr_ == nullptr || impl->audio_buffer_size_ == 0) {

            // 尝试从队列中取出一帧
            if (impl->audio_frame_queue_->try_pop(impl->current_audio_frame_)) {
                // 如果成功取出，立即诊断它！
                if (impl->current_audio_frame_) {

                    double frame_pts = impl->current_audio_frame_->pts;
                    // 打印我们拿到的真实 PTS 值
                    LOGI("AUDIO_CB: Popped new audio frame with PTS = %.3f", frame_pts);

                    if (frame_pts >= 0) {
                        LOGI("AUDIO_CB: PTS is valid. Updating master clock from %.3f to %.3f", impl->master_clock_pts_.load(), frame_pts);
                        impl->master_clock_pts_ = frame_pts;
                    } else {
                        LOGW("AUDIO_CB: Popped frame has invalid PTS (%.3f). Clock not updated.", frame_pts);
                    }
                } else {
                    LOGE("AUDIO_CB: try_pop succeeded but returned a null frame pointer!");
                }

                // 设置数据指针以供消费
                impl->audio_buffer_ptr_ = impl->current_audio_frame_->interleaved_pcm;
                impl->audio_buffer_size_ = impl->current_audio_frame_->interleaved_size;

            } else {
                // 如果队列为空，则跳出循环
                break;
            }
        }

        // --- 拷贝数据的逻辑保持不变 ---
        int bytesToCopy = std::min(bytesNeeded - bytesCopied, impl->audio_buffer_size_);
        memcpy(static_cast<uint8_t*>(audioData) + bytesCopied, impl->audio_buffer_ptr_, bytesToCopy);
        bytesCopied += bytesToCopy;
        impl->audio_buffer_ptr_ += bytesToCopy;
        impl->audio_buffer_size_ -= bytesToCopy;
    }

    if (bytesCopied < bytesNeeded) {
        // LOGI("AUDIO_CB: Padding with %d bytes of silence.", bytesNeeded - bytesCopied);
        memset(static_cast<uint8_t*>(audioData) + bytesCopied, 0, bytesNeeded - bytesCopied);
    }

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

void NativePlayer::Impl::handle_play(const CommandPlay& cmd)
{
    LOGI("FSM: Handling PLAY.");
    cleanup_resources();

    state_ = PlayerState::Seeking;
    if (on_state_changed_cb_)
        on_state_changed_cb_(state_);
    mp4parser::Config config;
    config.file_path = cmd.path;
    video_frame_queue_ = std::make_unique<player_utils::SemQueue<std::shared_ptr<VideoFrame>>>(30);
    audio_frame_queue_ = std::make_unique<player_utils::SemQueue<std::shared_ptr<AudioFrame>>>(60);
    master_clock_pts_ = 0.0;

    renderHost_ = GLRenderHost::create();
    if (!renderHost_->init(cmd.window)) {
        LOGE("FSM: GLRenderHost init failed.");
        state_ = PlayerState::End;
        if (on_state_changed_cb_)
            on_state_changed_cb_(state_);
        return;
    }

    std::weak_ptr<NativePlayer> weak_self = self_->shared_from_this();
    mp4parser::Callbacks callbacks;

    callbacks.on_video_frame_decoded = [this, weak_self](std::shared_ptr<VideoFrame> frame) {
        // LOGI("Tryinng push video frame.");

        if (auto self = weak_self.lock()) {
            // LOGI("Tryinng push video frame. -- into the lock");

            if (video_frame_queue_) {
                video_frame_queue_->push(std::move(frame));
            }
        }
        // LOGI("Tryinng push video frame. -- Done");
    };

    callbacks.on_audio_frame_decoded = [this, weak_self](std::shared_ptr<AudioFrame> frame) {
        // LOGI("Tryinng push Audio frame.");
        if (auto self = weak_self.lock()) {
            // LOGI("Tryinng push Audio frame. -- int he lock");
            if (audio_frame_queue_) {
                audio_frame_queue_->push(std::move(frame));
            }
            // LOGI("Tryinng push Audio frame. -- Done");
        }
    };

    callbacks.on_error = [weak_self](const std::string& msg) {
        if (auto strong_self = weak_self.lock()) {
            LOGE("Native error: %s", msg.c_str());
            if (strong_self->impl_->on_error_cb_) {
                strong_self->impl_->on_error_cb_(msg);
            }
            if (strong_self->impl_->on_state_changed_cb_) {
                strong_self->impl_->on_state_changed_cb_(PlayerState::Error);
            }
        }
    };

    parser_ = Mp4Parser::create(config, callbacks);
    if (!parser_) {
        LOGE("FSM: Mp4Parser creation failed.");
        cleanup_resources();
        state_ = PlayerState::End;
        if (on_state_changed_cb_)
            on_state_changed_cb_(state_);
        return;
    }

    // Audio render
    auto audio_params = parser_->getAudioParams();
    // 检查参数是否有效
    if (audio_params.sample_rate <= 0 || audio_params.channel_count <= 0) {
        LOGE("Failed to get valid audio parameters from parser. Aborting audio setup.");
        // 在这里，我们可以选择让播放继续（静音播放），或者直接报错停止
        // 为了安全，我们先报错停止
        cleanup_resources();
        state_ = PlayerState::End;
        return;
    }
    LOGI("Audio params retrieved: Rate=%d, Channels=%d", audio_params.sample_rate, audio_params.channel_count);

    audio_render_ = std::make_unique<AAudioRender>();
    audio_render_->configure(audio_params.sample_rate, audio_params.channel_count, AAUDIO_FORMAT_PCM_I16);
    audio_render_->setCallback(Impl::audio_data_callback, this);
    audio_render_->start();

    // 6. 启动解析器
    parser_->start();

    // 7. 切换到播放状态
    state_ = PlayerState::Playing;
    if (on_state_changed_cb_)
        on_state_changed_cb_(state_);
    LOGI("FSM: Switched to PLAYING state.");
}

void NativePlayer::Impl::handle_pause(const CommandPause& cmd)
{
    LOGI("FSM: Handling PAUSE (%d).", cmd.is_paused);
    if (parser_)
        parser_->pause();
    if (renderHost_) {
        cmd.is_paused ? renderHost_->pause() : renderHost_->resume();
    }
    if (audio_render_)
        audio_render_->pause(cmd.is_paused);

    state_ = cmd.is_paused ? PlayerState::Paused : PlayerState::Playing;
    if (on_state_changed_cb_)
        on_state_changed_cb_(state_);
}

void NativePlayer::Impl::handle_stop()
{
    LOGI("FSM: Handling STOP.");
    cleanup_resources();
    state_ = PlayerState::End;
    if (on_state_changed_cb_)
        on_state_changed_cb_(state_);
}

void NativePlayer::Impl::handle_seek(const CommandSeek& cmd)
{
    LOGI("FSM: Handling SEEK to %.2f.", cmd.position);
    if (parser_)
        parser_->seek(cmd.position);

    // 清空所有缓冲区
    if (renderHost_)
        renderHost_->flush();
    if (audio_render_)
        audio_render_->flush();
    if (video_frame_queue_)
        video_frame_queue_->clear();
    if (audio_frame_queue_)
        audio_frame_queue_->clear();

    // 重置主时钟
    master_clock_pts_ = cmd.position;
}

void NativePlayer::Impl::cleanup_resources()
{
    LOGI("FSM: Cleaning up resources.");
    if (parser_) {
        parser_->stop();
        parser_.reset();
    }

    if (audio_render_) {
        audio_render_->pause(true);
        audio_render_.reset();
    }
    if (renderHost_) {
        renderHost_->release();
        renderHost_.reset();
    }

    // 确保队列被清空和销毁
    if (video_frame_queue_)
        video_frame_queue_->shutdown();
    video_frame_queue_.reset();
    if (audio_frame_queue_)
        audio_frame_queue_->shutdown();
    audio_frame_queue_.reset();
}
void NativePlayer::Impl::run_sync_cycle()
{
    if (!video_frame_queue_ || video_frame_queue_->empty()) {
        // 这是正常情况，尤其是在播放开始或 seek 之后，表示在等待解码器缓冲
        // LOGD("SYNC: Video queue is empty, waiting for buffer."); // 可以用 LOGD 减少日志量
        return;
    }

    // 查看视频队列的头部帧，但不弹出
    std::optional<std::shared_ptr<VideoFrame>> video_frame_opt = video_frame_queue_->front();
    if (!video_frame_opt) {
        LOGW("SYNC: Queue not empty, but front() returned nullopt. Race condition?");
        return;
    }

    // 使用 const 引用避免不必要的拷贝
    const std::shared_ptr<VideoFrame>& video_frame = *video_frame_opt;
    if (!video_frame) {
        LOGE("SYNC: Popped a null video frame pointer!");
        // 从队列中移除这个坏数据
        std::shared_ptr<VideoFrame> dummy;
        video_frame_queue_->try_pop(dummy);
        return;
    }

    double video_pts = video_frame->pts;
    double master_clock = master_clock_pts_.load();
    double diff = video_pts - master_clock;

    // 视频帧的显示时间还未到
    if (diff > 0.01) {
        LOGI("SYNC: Video is early. VideoPTS=%.3f, AudioClock=%.3f, Diff=%.3f. Waiting...", video_pts, master_clock, diff);
        // 这里什么都不做，FSM循环的10ms超时会自动产生等待效果。
        return;
    }

    // --- 决定处理这一帧了 ---

    // 从队列中正式弹出这一帧。
    std::shared_ptr<VideoFrame> frame_to_process;
    if (!video_frame_queue_->try_pop(frame_to_process)) {
        LOGW("SYNC: front() had a frame, but try_pop() failed. Race condition?");
        return;
    }

    // 视频太晚了 (例如超过100ms)，直接丢弃
    if (diff < -0.1) {
        LOGW("SYNC: Video is too late. Dropping frame. VideoPTS=%.3f, AudioClock=%.3f, Diff=%.3f", video_pts, master_clock, diff);
        return; // 帧被弹出并丢弃
    }

    // 时间刚刚好或略晚，渲染它
    LOGI("SYNC: Submitting frame to renderer. VideoPTS=%.3f, AudioClock=%.3f, Diff=%.3f", video_pts, master_clock, diff);
    if (renderHost_) {
        // [BUG 已修复] 提交刚刚弹出的 frame_to_process，而不是可能已失效的 video_frame 引用
        renderHost_->submitFrame(std::move(frame_to_process));
    }
}