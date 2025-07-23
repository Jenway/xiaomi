#include "NativePlayer.hpp"
#include <android/log.h>

#define LOG_TAG "NativePlayer"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

NativePlayer::NativePlayer() = default;

NativePlayer::~NativePlayer()
{
    LOGI("NativePlayer destroyed");
    cleanup();
}

void NativePlayer::setOnStateChangedCallback(std::function<void(PlayerState)> cb)
{
    on_state_changed_cb_ = std::move(cb);
}

void NativePlayer::setOnErrorCallback(std::function<void(const std::string&)> cb)
{
    on_error_cb_ = std::move(cb);
}

int NativePlayer::play(const std::string& path, ANativeWindow* window)
{
    std::lock_guard<std::mutex> lock(player_mutex_);

    cleanup();

    // Setup Renderer
    if (!window) {
        LOGE("ANativeWindow is null");
        return -1;
    }
    renderHost_ = GLRenderHost::create();
    if (!renderHost_->init(window)) {
        LOGE("Failed to initialize GLRenderHost");
        cleanup();
        return -1;
    }

    // Setup Parser
    Config config;
    config.file_path = path;

    Callbacks callbacks;
    callbacks.on_frame_decoded = [this](const std::shared_ptr<VideoFrame>& frame) {
        if (frame && renderHost_) {
            renderHost_->submitFrame(frame);
        }
    };
    callbacks.on_state_changed = [this](PlayerState state) {
        LOGI("Player state changed to: %d", static_cast<int>(state));
        if (on_state_changed_cb_) {
            on_state_changed_cb_(state);
        }
    };
    callbacks.on_error = [this](const std::string& msg) {
        LOGE("Native error: %s", msg.c_str());
        if (on_error_cb_) {
            on_error_cb_(msg);
        }
        if (on_state_changed_cb_) {
            on_state_changed_cb_(PlayerState::Error);
        }
    };

    parser_ = Mp4Parser::create(config, callbacks);
    if (!parser_) {
        LOGE("Failed to create Mp4Parser");
        cleanup();
        return -1;
    }
    // Parser 自己会启动两个线程
    parser_->start();
    return 0;
}

void NativePlayer::pause(bool is_paused)
{
    std::lock_guard<std::mutex> lock(player_mutex_);
    if (!parser_)
        return;
    if (is_paused) {
        parser_->pause();
    } else {
        parser_->resume();
    }
}

void NativePlayer::stop()
{

    std::unique_ptr<GLRenderHost> host_to_release;
    std::unique_ptr<Mp4Parser> parser_to_stop;

    {
        std::lock_guard<std::mutex> lock(player_mutex_);
        host_to_release = std::move(renderHost_);
        parser_to_stop = std::move(parser_);
    }

    if (parser_to_stop) {
        parser_to_stop->stop();
    }
    if (host_to_release) {
        host_to_release->release();
    }

    LOGI("NativePlayer cleaned up.");
}

void NativePlayer::onDrawFrame()
{
    if (renderHost_) {
        renderHost_->drawFrame();
    }
}

void NativePlayer::cleanup()
{
    if (parser_) {
        parser_->stop();
        parser_.reset();
    }
    if (renderHost_) {
        renderHost_->release();
        renderHost_.reset();
    }
    LOGI("NativePlayer cleaned up.");
}