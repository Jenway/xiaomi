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
#include <queue>
#include <thread>
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
            lock.lock(); // 再次锁定以检查下一个命令
        }

        if (shutdown_requested_)
            break;

        // --- 3. 如果处于播放状态，则执行音视频同步 ---
        if (state_.load() == PlayerState::Playing) {
            lock.unlock(); // 解锁以执行同步逻辑
            run_sync_cycle();
        }
    }
    LOGI("FSM thread finished.");
}

// --- 实现内部处理函数 ---

int NativePlayer::Impl::audio_data_callback(AAudioStream* stream, void* userData, void* audioData, int32_t numFrames)
{
    auto* impl = static_cast<NativePlayer::Impl*>(userData);
    if (!impl || impl->state_ == PlayerState::Stopped) {
        return 1; // 停止回调
    }

    std::shared_ptr<AudioFrame> frame;
    // 尝试非阻塞地从队列中取出一帧
    if (impl->audio_frame_queue_ && impl->audio_frame_queue_->try_pop(frame)) {
        // 假设 frame->data 包含足够的PCM数据
        // 在真实项目中，你需要一个缓冲区管理器来处理数据不足/超出的情况
        memcpy(audioData, frame->data[0], frame->linesize[0]);

        // 关键：根据消耗的音频帧时长，推进主时钟
        impl->master_clock_pts_ = frame->duration + impl->master_clock_pts_.load();
    } else {
        // 队列为空，填充静音以避免杂音
        // 假设音频格式为 16-bit stereo
        memset(audioData, 0, numFrames * 2 * sizeof(int16_t));
    }

    return 0; // 继续回调
}

void NativePlayer::Impl::handle_play(const CommandPlay& cmd)
{
    LOGI("FSM: Handling PLAY.");
    cleanup_resources(); // 清理旧资源

    // 切换到中间状态
    state_ = PlayerState::Seeking; // 可以认为是 "Preparing"
    if (on_state_changed_cb_)
        on_state_changed_cb_(state_);

    // --- 初始化队列和时钟 ---
    video_frame_queue_ = std::make_unique<player_utils::SemQueue<std::shared_ptr<VideoFrame>>>(30); // 视频缓冲30帧
    audio_frame_queue_ = std::make_unique<player_utils::SemQueue<std::shared_ptr<AudioFrame>>>(60); // 音频缓冲60帧
    master_clock_pts_ = 0.0;

    // --- 创建渲染器 ---
    renderHost_ = GLRenderHost::create();
    if (!renderHost_->init(cmd.window)) {
        LOGE("FSM: GLRenderHost init failed.");
        state_ = PlayerState::End; // 出错，回到结束状态
        if (on_state_changed_cb_)
            on_state_changed_cb_(state_);
        return;
    }
    // audio render
    // --- 创建音频渲染器 ---
    audio_render_ = std::make_unique<AAudioRender>();
    // TODO: 应该从Parser获取正确的音频参数
    audio_render_->configure(44100, 2, AAUDIO_FORMAT_PCM_I16);
    audio_render_->setCallback(Impl::audio_data_callback, this);
    audio_render_->start();

    // --- 创建解析器 ---
    // 使用 weak_ptr 防止回调中的 use-after-free
    std::weak_ptr<NativePlayer> weak_self = self_->shared_from_this();
    mp4parser::Config config;
    config.file_path = cmd.path;
    mp4parser::Callbacks callbacks;

    callbacks.on_video_frame_decoded = [this, weak_self](const auto& frame) {
        if (auto self = weak_self.lock()) { // 检查 NativePlayer 是否还活着
            if (renderHost_)
                renderHost_->submitFrame(frame);
        }
    };
    // 回调：将解码的音频帧放入音频队列
    callbacks.on_audio_frame_decoded = [this, weak_self](const auto& frame) {
        if (auto self = weak_self.lock()) {
            if (audio_frame_queue_)
                audio_frame_queue_->push(frame);
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

    parser_->start();

    state_ = PlayerState::Playing; // 成功，进入播放状态
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
    std::shared_ptr<VideoFrame> video_frame;

    // 查看视频队列的头部而不弹出
    if (!video_frame_queue_ || !video_frame_queue_->front()) {
        return; // 视频队列为空，等待缓冲
    }

    double video_pts = video_frame->pts;
    double master_clock = master_clock_pts_.load();
    double diff = video_pts - master_clock;

    if (diff > 0.01) {
        // 视频早了，等待。这里什么都不做，FSM循环的10ms超时会自动产生等待效果。
        return;
    }

    // 视频时间已到或已晚，弹出这一帧
    video_frame_queue_->try_pop(video_frame);

    if (diff < -0.1) {
        // 视频太晚了 (超过100ms)，直接丢弃，不渲染
        LOGW("Dropping late video frame. PTS: %.3f, Clock: %.3f", video_pts, master_clock);
        return;
    }

    // 时间刚刚好或略晚，渲染它
    if (video_frame && renderHost_) {
        renderHost_->submitFrame(video_frame);
    }
}