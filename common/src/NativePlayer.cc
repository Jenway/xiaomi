#include "NativePlayer.hpp"
#include "AAudioRender.h"
#include "Entitys.hpp"
#include "GLRenderHost.hpp"
#include "JniCallbackHandler.hpp"
#include "MediaPipeline.hpp"
#include "Mp4Parser.hpp"
#include "SemQueue.hpp"
#include "SyncClock.hpp"
#include <aaudio/AAudio.h>
#include <android/log.h>
#include <android/native_window.h>
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

using player_utils::AudioFrame;
using player_utils::PlayerState;
using player_utils::SemQueue;
using player_utils::VideoFrame;
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

struct CommandSetSpeed {
    float speed;
};

struct AudioCallbackState {
    std::atomic<bool> is_active { true };

    SemQueue<shared_ptr<AudioFrame>>* audio_frame_queue {};
    SyncClock* clock {}; // 用于更新主时钟
    std::atomic<bool>* is_logically_paused {};

    // 缓冲状态
    std::shared_ptr<AudioFrame> current_audio_frame_;
    uint8_t* audio_buffer_ptr_ = nullptr;
    int audio_buffer_size_ = 0;
};
// 放在 NativePlayer::Impl 的定义之上
class AudioCallbackGuard {
public:
    explicit AudioCallbackGuard(AudioCallbackState* state)
        : state_(state)
    {
        if (state_) {
            // 进入危险区域，关闭回调
            state_->is_active.store(false);
            // 等待，确保正在执行的回调能完成
            std::this_thread::sleep_for(std::chrono::milliseconds(20)); // 等待时间可以缩短
        }
    }

    ~AudioCallbackGuard()
    {
        if (state_) {
            // 离开危险区域，重新打开回调
            state_->is_active.store(true);
        }
    }

    // 禁止拷贝
    AudioCallbackGuard(const AudioCallbackGuard&) = delete;
    AudioCallbackGuard& operator=(const AudioCallbackGuard&) = delete;

private:
    AudioCallbackState* state_;
};
using Command = std::variant<
    CommandPlay,
    CommandPause,
    CommandStop,
    CommandSeek,
    CommandSetSpeed,
    CommandShutdown>;

struct NativePlayer::Impl {
    explicit Impl(NativePlayer* self);
    ~Impl();

    void fsm_loop();
    void set_state(PlayerState new_state);

    // --- 状态和线程 ---
    NativePlayer* self_;
    std::atomic<PlayerState> state_ { PlayerState::None };
    std::thread fsm_thread_;
    std::queue<Command> command_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cond_;
    bool shutdown_requested_ = false;

    // --- core ---
    unique_ptr<MediaPipeline> pipeline_;
    unique_ptr<AudioCallbackState> audio_cb_state_;
    unique_ptr<SyncClock> clock_;
    unique_ptr<JniCallbackHandler> jni_handler_;

    // --- 回调 ---
    static int audio_data_callback(AAudioStream* stream, void* userData, void* audioData, int32_t numFrames);
    std::function<void(PlayerState)> on_state_changed_cb_;
    std::function<void(const std::string&)> on_error_cb_;

    std::atomic<bool> is_logically_paused_ { false };

private:
    void handle_play(const CommandPlay& cmd);
    void handle_pause(const CommandPause& cmd);
    void handle_stop();
    void handle_seek(const CommandSeek& cmd);
    void handle_set_speed(const CommandSetSpeed& cmd);
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
void NativePlayer::setJniEnv(JavaVM* vm, jobject player_object)
{
    JNIEnv* env = nullptr;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        LOGE("Failed to get JNIEnv in setJniEnv");
        return;
    }
    jobject global_player_ref = env->NewGlobalRef(player_object);
    if (global_player_ref == nullptr) {
        LOGE("Failed to create global reference for player object");
        return;
    }

    impl_->jni_handler_ = std::make_unique<JniCallbackHandler>(vm, global_player_ref);
    LOGI("JniCallbackHandler created.");
}

void NativePlayer::play(const std::string& path, ANativeWindow* window)
{
    LOGI("Dispatching PLAY command.");
    if (window != nullptr) {
        ANativeWindow_acquire(window);
        LOGI("ANativeWindow acquired in play().");
    }
    {
        std::lock_guard lock(impl_->queue_mutex_);
        impl_->command_queue_.emplace(CommandPlay { path, window });
    }
    impl_->queue_cond_.notify_one();
}

void NativePlayer::pause(bool is_paused)
{
    LOGI("Dispatching PAUSE command with is_paused = %d", is_paused);
    {
        std::lock_guard lock(impl_->queue_mutex_);
        impl_->command_queue_.emplace(CommandPause { is_paused });
    }
    impl_->queue_cond_.notify_one();
}

void NativePlayer::stop()
{
    LOGI("Dispatching STOP command.");
    {
        std::lock_guard lock(impl_->queue_mutex_);
        impl_->command_queue_.emplace(CommandStop {});
    }
    impl_->queue_cond_.notify_one();
}

void NativePlayer::seek(double time_sec)
{
    LOGI("Dispatching SEEK command.");
    {
        std::lock_guard lock(impl_->queue_mutex_);
        impl_->command_queue_.emplace(CommandSeek { time_sec });
    }
    impl_->queue_cond_.notify_one();
}

double NativePlayer::getDuration() const
{
    if (impl_ && impl_->pipeline_) {
        return impl_->pipeline_->getDuration();
    }
    return 0.0;
}

void NativePlayer::setSpeed(float speed)
{
    LOGI("Dispatching SET_SPEED command with speed = %.2f", speed);
    {
        std::lock_guard lock(impl_->queue_mutex_);
        impl_->command_queue_.emplace(CommandSetSpeed { speed });
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
        // --- 等待事件 ---
        std::unique_lock lock(queue_mutex_);
        if (state_.load() == PlayerState::Playing) {
            // 播放时，以10ms为超时进行等待。超时后可以执行一轮同步逻辑。
            queue_cond_.wait_for(lock, std::chrono::milliseconds(10), [this] { return !command_queue_.empty(); });
        } else {
            // 其他状态下，无限等待命令。
            queue_cond_.wait(lock, [this] { return !command_queue_.empty(); });
        }

        while (!command_queue_.empty()) {
            Command cmd = std::move(command_queue_.front());
            command_queue_.pop();
            lock.unlock();

            if (std::holds_alternative<CommandShutdown>(cmd)) {
                LOGI("FSM received SHUTDOWN command. Exiting loop.");
                cleanup_resources();
                shutdown_requested_ = true;
                break;
            }

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
                } else if (std::holds_alternative<CommandSetSpeed>(cmd)) {
                    handle_set_speed(std::get<CommandSetSpeed>(cmd));
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

        // --- 如果处于播放状态，则执行音视频同步 ---
        if (state_.load() == PlayerState::Playing) {
            lock.unlock();
            run_sync_cycle();
        }
    }
    LOGI("FSM thread finished.");
}

void NativePlayer::Impl::set_state(PlayerState new_state)
{
    if (state_ == new_state)
        return;

    state_ = new_state;
    LOGI("State changed to: %d", static_cast<int>(new_state));

    if (on_state_changed_cb_) {
        on_state_changed_cb_(new_state);
    }

    if (jni_handler_) {
        jni_handler_->notifyStateChanged(new_state);
    }
}

void NativePlayer::Impl::handle_play(const CommandPlay& cmd)
{
    LOGI("FSM: Handling PLAY.");
    cleanup_resources();

    // --- Core ---
    pipeline_ = std::make_unique<MediaPipeline>();
    clock_ = std::make_unique<SyncClock>();
    audio_cb_state_ = std::make_unique<AudioCallbackState>();

    // --- callbacks ---
    mp4parser::Callbacks callbacks;

    callbacks.on_video_frame_decoded = [this](auto frame) {
        if (pipeline_ && pipeline_->video_frame_queue_) {
            pipeline_->video_frame_queue_->push(std::move(frame));
        }
    };
    callbacks.on_audio_frame_decoded = [this](auto frame) {
        if (pipeline_ && pipeline_->audio_frame_queue_) {
            pipeline_->audio_frame_queue_->push(std::move(frame));
        }
    };

    std::weak_ptr<NativePlayer> weak_self = self_->shared_from_this();
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

    set_state(PlayerState::Seeking);

    // --- 初始化 pipeline ---
    mp4parser::Config config;
    config.file_path = cmd.path;

    if (!pipeline_->initialize(config, cmd.window, callbacks)) {
        LOGE("FSM: MediaPipeline initialization failed.");
        cleanup_resources();
        set_state(PlayerState::End);
        return;
    }

    // --- Audio callback ---
    audio_cb_state_->audio_frame_queue = pipeline_->audio_frame_queue_.get();
    audio_cb_state_->clock = clock_.get();
    audio_cb_state_->is_logically_paused = &is_logically_paused_;

    pipeline_->audio_render_->setCallback(Impl::audio_data_callback, audio_cb_state_.get());

    pipeline_->start();

    set_state(PlayerState::Playing);
}

void NativePlayer::Impl::handle_set_speed(const CommandSetSpeed& cmd)
{
    LOGI("DO NOTHING NOW");
}

void NativePlayer::Impl::handle_pause(const CommandPause& cmd)
{
    LOGI("FSM: Handling PAUSE (%d).", cmd.is_paused);
    is_logically_paused_ = cmd.is_paused;

    if (pipeline_) {
        pipeline_->pause(cmd.is_paused);
    }

    set_state(cmd.is_paused ? PlayerState::Paused : PlayerState::Playing);
}

void NativePlayer::Impl::handle_stop()
{
    LOGI("FSM: Handling STOP.");
    cleanup_resources();
    set_state(PlayerState::End);
}

void NativePlayer::Impl::handle_seek(const CommandSeek& cmd)
{
    LOGI("FSM: Handling SEEK to %.2f.", cmd.position);

    set_state(PlayerState::Seeking);

    if (pipeline_) {
        pipeline_->seek(cmd.position);
        if (pipeline_->video_frame_queue_)
            pipeline_->video_frame_queue_->clear();
        if (pipeline_->audio_frame_queue_)
            pipeline_->audio_frame_queue_->clear();
        pipeline_->flush();
    }

    if (clock_) {
        clock_->reset(cmd.position);
    }

    set_state(is_logically_paused_ ? PlayerState::Paused : PlayerState::Playing);
}

void NativePlayer::Impl::cleanup_resources()
{
    LOGI("FSM: Cleaning up resources, starting shutdown sequence...");

    if (pipeline_) {
        pipeline_->stop();
        pipeline_.reset();
    }

    if (clock_) {
        clock_.reset();
    }

    if (audio_cb_state_) {
        audio_cb_state_.reset();
    }

    LOGI("FSM: All resources have been cleaned up.");
}

PlayerState NativePlayer::getState() const
{
    if (impl_) {
        return impl_->state_.load();
    }
    return PlayerState::None;
}

double NativePlayer::getPosition() const
{
    if (impl_ && impl_->clock_) {
        return impl_->clock_->get();
    }
    return 0.0;
}
void NativePlayer::Impl::run_sync_cycle()
{
    if (!pipeline_->video_frame_queue_ || pipeline_->video_frame_queue_->empty()) {
        // LOGD("SYNC: Video queue is empty, waiting for buffer.");
        return;
    }

    std::optional<std::shared_ptr<VideoFrame>> video_frame_opt = pipeline_->video_frame_queue_->front();
    if (!video_frame_opt) {
        LOGW("SYNC: Queue not empty, but front() returned nullopt. Race condition?");
        return;
    }

    const std::shared_ptr<VideoFrame>& video_frame = *video_frame_opt;
    if (!video_frame) {
        LOGE("SYNC: Popped a null video frame pointer!");
        std::shared_ptr<VideoFrame> dummy;
        pipeline_->video_frame_queue_->try_pop(dummy);
        return;
    }

    auto decision = clock_->checkVideoFrame(video_frame->pts);

    switch (decision) {
    case SyncClock::SyncDecision::Wait:
    case SyncClock::SyncDecision::Drop:
        return;
    case SyncClock::SyncDecision::Render:
        break;
    }

    std::shared_ptr<VideoFrame> frame_to_process;
    if (!pipeline_->video_frame_queue_->try_pop(frame_to_process)) {
        LOGW("SYNC: front() had a frame, but try_pop() failed. Race condition?");
        return;
    }
    if (pipeline_->video_render_) {
        pipeline_->video_render_->submitFrame(std::move(frame_to_process));
    }
}

int NativePlayer::Impl::audio_data_callback(AAudioStream* stream, void* userData, void* audioData, int32_t numFrames)
{
    auto* state = static_cast<AudioCallbackState*>(userData);
    if (!state || !state->is_active.load()) {
        // 填充静音，避免爆音
        int32_t bytesNeeded = numFrames * AAudioStream_getChannelCount(stream) * sizeof(int16_t); // 假设是 PCM_I16
        memset(audioData, 0, bytesNeeded);
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

    if (!state->audio_frame_queue || !state->clock) {
        return AAUDIO_CALLBACK_RESULT_STOP;
    }

    int32_t channelCount = AAudioStream_getChannelCount(stream);
    int32_t format = AAudioStream_getFormat(stream);
    int32_t bytesPerSample = (format == AAUDIO_FORMAT_PCM_I16) ? sizeof(int16_t) : sizeof(float);
    int32_t bytesNeeded = numFrames * channelCount * bytesPerSample;
    int32_t bytesCopied = 0;

    if (state->is_logically_paused->load()) {
        // 如果是暂停状态，填充静音，并且不从队列取数据，也不推进时钟
        // soft pause
        memset(audioData, 0, bytesNeeded);
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

    while (bytesCopied < bytesNeeded) {
        if (state->audio_buffer_ptr_ == nullptr || state->audio_buffer_size_ == 0) {

            if (state->audio_frame_queue->try_pop(state->current_audio_frame_)) {
                if (state->current_audio_frame_) {

                    double frame_pts = state->current_audio_frame_->pts;
                    LOGI("AUDIO_CB: Popped new audio frame with PTS = %.3f", frame_pts);

                    if (frame_pts >= 0) {
                        LOGI("AUDIO_CB: PTS is valid. Updating master clock from %.3f to %.3f", state->clock->get(), frame_pts);
                        state->clock->update(frame_pts);
                    } else {
                        LOGW("AUDIO_CB: Popped frame has invalid PTS (%.3f). Clock not updated.", frame_pts);
                    }
                } else {
                    LOGE("AUDIO_CB: try_pop succeeded but returned a null frame pointer!");
                }

                state->audio_buffer_ptr_ = state->current_audio_frame_->interleaved_pcm;
                state->audio_buffer_size_ = state->current_audio_frame_->interleaved_size;

            } else {
                break;
            }
        }

        int bytesToCopy = std::min(bytesNeeded - bytesCopied, state->audio_buffer_size_);
        memcpy(static_cast<uint8_t*>(audioData) + bytesCopied, state->audio_buffer_ptr_, bytesToCopy);
        bytesCopied += bytesToCopy;
        state->audio_buffer_ptr_ += bytesToCopy;
        state->audio_buffer_size_ -= bytesToCopy;
    }

    // 不够的话就填充静音
    if (bytesCopied < bytesNeeded) {
        memset(static_cast<uint8_t*>(audioData) + bytesCopied, 0, bytesNeeded - bytesCopied);
    }

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}
