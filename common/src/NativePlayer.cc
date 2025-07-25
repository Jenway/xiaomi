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
#include <sstream> // Required for std::stringstream
#include <thread>
#include <utility>
#include <variant>

// Helper to get thread ID as a string
inline std::string get_thread_id_str()
{
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

#undef LOG_TAG
#define LOG_TAG "NativePlayerFSM"
#define LOG_BUFFER_SIZE 1024

#define LOGE(...)                                                                                                           \
    do {                                                                                                                    \
        char buf[LOG_BUFFER_SIZE];                                                                                          \
        snprintf(buf, LOG_BUFFER_SIZE, __VA_ARGS__);                                                                        \
        __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, "[TID:%s] %s: %s", get_thread_id_str().c_str(), __FUNCTION__, buf); \
    } while (0)

#define LOGI(...)                                                                                                          \
    do {                                                                                                                   \
        char buf[LOG_BUFFER_SIZE];                                                                                         \
        snprintf(buf, LOG_BUFFER_SIZE, __VA_ARGS__);                                                                       \
        __android_log_print(ANDROID_LOG_INFO, LOG_TAG, "[TID:%s] %s: %s", get_thread_id_str().c_str(), __FUNCTION__, buf); \
    } while (0)

#define LOGW(...)                                                                                                          \
    do {                                                                                                                   \
        char buf[LOG_BUFFER_SIZE];                                                                                         \
        snprintf(buf, LOG_BUFFER_SIZE, __VA_ARGS__);                                                                       \
        __android_log_print(ANDROID_LOG_WARN, LOG_TAG, "[TID:%s] %s: %s", get_thread_id_str().c_str(), __FUNCTION__, buf); \
    } while (0)

#define LOGD(...)                                                                                                           \
    do {                                                                                                                    \
        char buf[LOG_BUFFER_SIZE];                                                                                          \
        snprintf(buf, LOG_BUFFER_SIZE, __VA_ARGS__);                                                                        \
        __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, "[TID:%s] %s: %s", get_thread_id_str().c_str(), __FUNCTION__, buf); \
    } while (0)

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
    bool video_first_frame_rendered = false;
    bool audio_started = false;
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
    std::atomic<bool> video_first_frame_rendered_ = false;
    std::atomic<bool> audio_started_ = false;

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
            LOGD("Video frame decoded callback triggered. PTS: %.3f", frame->pts);
            return pipeline_->video_frame_queue_->push(std::move(frame));
        }
    };
    callbacks.on_audio_frame_decoded = [this](auto frame) {
        if (pipeline_ && pipeline_->audio_frame_queue_) {
            return pipeline_->audio_frame_queue_->push(std::move(frame));
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

// 在 NativePlayer::Impl 中
void NativePlayer::Impl::handle_seek(const CommandSeek& cmd)
{
    LOGI("FSM: Handling SEEK to %.2f. Orchestrating shutdown sequence...", cmd.position);

    // 1. 立即暂停音频输出，这是最外层的消费者
    if (pipeline_ && pipeline_->audio_render_) {
        pipeline_->audio_render_->pause(true);
    }
    set_state(PlayerState::Seeking);

    // 2. 【核心】关闭“下游”的帧队列 (Frame Queues)。
    // 这会解除解码器线程的阻塞，如果它们正卡在 push() 操作上的话。
    LOGI("Seek Orchestrator: Shutting down FRAME queues...");
    if (pipeline_ && pipeline_->video_frame_queue_) {
        pipeline_->video_frame_queue_->shutdown();
    }
    if (pipeline_ && pipeline_->audio_frame_queue_) {
        pipeline_->audio_frame_queue_->shutdown();
    }

    // 3. 现在，向下游的 Mp4Parser 发送 seek 命令。
    // Mp4Parser 内部的 handle_seek 逻辑（关闭 packet_queue -> join 线程）现在可以安全执行了，
    // 因为我们已经从外部解除了它解码器线程的最大阻塞源。
    if (pipeline_) {
        auto promise = std::make_shared<std::promise<void>>();
        pipeline_->seek(cmd.position, promise);

        LOGI("FSM thread is now BLOCKED, waiting for pipeline (Mp4Parser) seek to complete...");
        auto future = promise->get_future();
        future.wait(); // 等待 Mp4Parser 完成它的内部 seek 流程
        LOGI("FSM thread UNBLOCKED. Mp4Parser has finished its seek operation.");
    }

    // 4. Mp4Parser 已经停止了它的线程。现在我们重置“下游”的帧队列，为播放做准备。
    LOGI("Seek Orchestrator: Clearing potentially stale frames from queues...");
    if (pipeline_ && pipeline_->video_frame_queue_) {
        pipeline_->video_frame_queue_->clear(); // 使用你的 clear 方法
    }
    if (pipeline_ && pipeline_->audio_frame_queue_) {
        pipeline_->audio_frame_queue_->clear();
    }

    // 现在才重置队列，让它们准备好接收 seek 之后的新数据
    LOGI("Seek Orchestrator: Resetting FRAME queues...");
    if (pipeline_ && pipeline_->video_frame_queue_) {
        pipeline_->video_frame_queue_->reset();
    }
    if (pipeline_ && pipeline_->audio_frame_queue_) {
        pipeline_->audio_frame_queue_->reset();
    }

    // 5. 清理渲染器中的残留帧
    LOGI("Seek Orchestrator: Flushing renderers.");
    if (pipeline_) {
        pipeline_->flush();
    }

    // 6. 重置主时钟
    if (clock_) {
        clock_->reset(cmd.position);
    }

    // 7. 如果不是逻辑暂停状态，则恢复音频播放
    if (!is_logically_paused_.load()) {
        if (pipeline_ && pipeline_->audio_render_) {
            pipeline_->audio_render_->pause(false);
        }
    }

    // 8. 设置最终状态
    set_state(is_logically_paused_ ? PlayerState::Paused : PlayerState::Playing);
    LOGI("Seek orchestration complete. Player state is now %s.", (is_logically_paused_ ? "Paused" : "Playing"));
}

void NativePlayer::Impl::cleanup_resources()
{
    LOGI("FSM: Cleaning up resources, starting shutdown sequence...");

    // --- FIX: Explicitly pause the stream before doing anything else. ---
    if (pipeline_ && pipeline_->audio_render_) {
        pipeline_->audio_render_->pause(true);
    }

    // The guard is still useful to ensure no callback logic runs while we reset pointers.
    AudioCallbackGuard cb_guard(audio_cb_state_.get());

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
        return;
    case SyncClock::SyncDecision::Drop: {
        std::shared_ptr<VideoFrame> dropped;
        pipeline_->video_frame_queue_->try_pop(dropped);
        LOGW("SYNC: Dropped frame PTS=%.3f", video_frame->pts);
        return;
    }
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
        audio_cb_state_->video_first_frame_rendered = true;
    }
}
int NativePlayer::Impl::audio_data_callback(AAudioStream* stream,
    void* userData,
    void* audioData,
    int32_t numFrames)
{
    auto* state = static_cast<AudioCallbackState*>(userData);
    if (!state || !state->is_active.load()) {
        return AAUDIO_CALLBACK_RESULT_STOP;
    }

    int32_t channelCount = AAudioStream_getChannelCount(stream);
    int32_t bytesPerSample = sizeof(int16_t);
    int32_t bytesNeeded = numFrames * channelCount * bytesPerSample;
    auto* outputBuffer = static_cast<uint8_t*>(audioData);

    if (!state->audio_started) {
        if (!state->video_first_frame_rendered) {
            memset(outputBuffer, 0, bytesNeeded);
            return AAUDIO_CALLBACK_RESULT_CONTINUE;
        }
        state->audio_started = true;
        LOGI("AUDIO_CB: Primera trama de vídeo renderizada; iniciando reloj/salida de audio.");
    }

    if (state->is_logically_paused->load()) {
        memset(outputBuffer, 0, bytesNeeded);
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    }

    int32_t bytesCopied = 0;
    while (bytesCopied < bytesNeeded) {
        if (state->audio_buffer_size_ == 0) {
            if (state->audio_frame_queue->try_pop(state->current_audio_frame_) && state->current_audio_frame_) {
                state->audio_buffer_ptr_ = state->current_audio_frame_->interleaved_pcm;
                state->audio_buffer_size_ = state->current_audio_frame_->interleaved_size;

                double pts = state->current_audio_frame_->pts;
                if (pts >= 0) {
                    state->clock->update(pts);
                }
            } else {
                LOGW("AUDIO_CB: La cola de audio está vacía. Rellenando con silencio.");
                if (bytesCopied > 0) {
                    memset(outputBuffer + bytesCopied, 0, bytesNeeded - bytesCopied);
                } else {
                    memset(outputBuffer, 0, bytesNeeded);
                }
                return AAUDIO_CALLBACK_RESULT_CONTINUE;
            }
        }

        int32_t chunk = std::min(bytesNeeded - bytesCopied, state->audio_buffer_size_);
        memcpy(outputBuffer + bytesCopied, state->audio_buffer_ptr_, chunk);

        state->audio_buffer_ptr_ += chunk;
        state->audio_buffer_size_ -= chunk;
        bytesCopied += chunk;
    }

    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}