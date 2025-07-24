#include "NativePlayer.hpp"
#include <android/log.h>

#define LOG_TAG "NativePlayer"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

NativePlayer::NativePlayer() = default;

NativePlayer::~NativePlayer()
{
    LOGI("NativePlayer destroyed");
    stop();
}

void NativePlayer::setOnStateChangedCallback(std::function<void(PlayerState)> cb)
{
    on_state_changed_cb_ = std::move(cb);
}

void NativePlayer::setOnErrorCallback(std::function<void(const std::string&)> cb)
{
    on_error_cb_ = std::move(cb);
}

void NativePlayer::seek(double time_sec)
{
    std::lock_guard<std::mutex> lock(player_mutex_);

    if (!parser_ || !renderHost_) {
        LOGI("Cannot seek, player is not in a valid state.");
        return;
    }

    LOGI("NativePlayer seeking to %.3f seconds.", time_sec);

    parser_->seek(time_sec);
    renderHost_->flush();

    LOGI("NativePlayer seek operation completed.");
}

int NativePlayer::play(const std::string& path, ANativeWindow* window)
{
    LOGI("Play command received for path: %s", path.c_str());

    // 1. 调用 stop() 来清理任何旧的播放会话，而不是 cleanup()
    stop();
    std::lock_guard<std::mutex> lock(player_mutex_);

    // 2. 初始化 GLRenderHost，它会自动启动自己的渲染线程
    if (!window) {
        LOGE("ANativeWindow is null");
        return -1;
    }
    renderHost_ = GLRenderHost::create();
    if (!renderHost_->init(window)) {
        LOGE("Failed to initialize GLRenderHost");
        stop(); // 使用 stop 清理部分初始化的资源
        return -1;
    }

    // 3. 设置解析器和回调
    Config config;
    config.file_path = path;
    Callbacks callbacks;
    callbacks.on_frame_decoded = [this](const std::shared_ptr<VideoFrame>& frame) {
        // 这个回调保持不变，它将解码后的帧喂给渲染器
        if (frame && renderHost_) {
            renderHost_->submitFrame(frame);
        }
    };
    callbacks.on_state_changed = [this](PlayerState state) {
        if (on_state_changed_cb_)
            on_state_changed_cb_(state);
    };
    callbacks.on_error = [this](const std::string& msg) {
        LOGE("Native error from parser: %s", msg.c_str());
        if (on_error_cb_)
            on_error_cb_(msg);
        if (on_state_changed_cb_)
            on_state_changed_cb_(PlayerState::Error);
    };

    parser_ = Mp4Parser::create(config, callbacks);
    if (!parser_) {
        LOGE("Failed to create Mp4Parser");
        stop(); // 使用 stop 清理
        return -1;
    }

    // 4. 启动解析器线程
    parser_->start();

    LOGI("NativePlayer is now playing.");
    return 0;
}
void NativePlayer::pause(bool is_paused)
{
    std::lock_guard<std::mutex> lock(player_mutex_);

    LOGI("Pause command received: %s", is_paused ? "true" : "false");

    if (is_paused) {
        if (parser_)
            parser_->pause();
        if (renderHost_)
            renderHost_->pause(); // <-- 新增: 暂停渲染循环
    } else {
        if (parser_)
            parser_->resume();
        if (renderHost_)
            renderHost_->resume(); // <-- 新增: 恢复渲染循环
    }
}

// 在 NativePlayer.cpp 中修改 stop()

void NativePlayer::stop()
{
    LOGI("Stop command received.");

    std::unique_ptr<GLRenderHost> host_to_release;
    std::unique_ptr<Mp4Parser> parser_to_stop;

    // --- 临界区开始 ---
    // 这个代码块的目的是“抢夺”清理资源的所有权
    // 它会非常快地执行完毕
    {
        std::lock_guard<std::mutex> lock(player_mutex_);

        // 如果指针已经是空的，说明另一个线程已经拿走了它们去清理了
        // 我们就可以直接返回，避免重复工作
        if (!parser_ && !renderHost_) {
            LOGI("Cleanup seems to be already in progress or finished by another thread. Ignoring call.");
            return;
        }

        // 使用 std::move 把资源的所有权转移到局部变量
        host_to_release = std::move(renderHost_);
        parser_to_stop = std::move(parser_);
    }
    // --- 临界区结束，锁被释放 ---

    // --- 耗时的清理操作 ---
    // 这些操作现在在锁的外部执行，不会阻塞其他想要获取锁的线程
    if (parser_to_stop) {
        LOGI("Stopping Mp4Parser...");
        parser_to_stop->stop(); // 这个 join() 可能会耗时
    }
    if (host_to_release) {
        LOGI("Releasing GLRenderHost...");
        host_to_release->release(); // 这个 join() 可能会耗时
    }

    LOGI("NativePlayer stop process finished.");
}