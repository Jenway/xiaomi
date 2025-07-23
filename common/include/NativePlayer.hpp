#pragma once

#include "Entitys.hpp"
#include "GLRenderHost.hpp"
#include "Mp4Parser.hpp"
#include <android/native_window.h>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

using namespace mp4parser;
using namespace render_utils;
using player_utils::VideoFrame;

class NativePlayer {
public:
    NativePlayer();
    ~NativePlayer();

    // 禁止拷贝和赋值
    NativePlayer(const NativePlayer&) = delete;
    NativePlayer& operator=(const NativePlayer&) = delete;

    // --- Public API ---
    // 这个方法底层将启动两个线程（Demuxer & Decoder）
    int play(const std::string& path, ANativeWindow* window);
    void pause(bool is_paused);
    void stop();
    // 这个方法将会被渲染线程调用
    void onDrawFrame();

    void seek(double time_sec);

    // --- Callbacks Setup ---
    void setOnStateChangedCallback(std::function<void(PlayerState)> cb);
    void setOnErrorCallback(std::function<void(const std::string&)> cb);

private:
    void cleanup();

    std::unique_ptr<Mp4Parser> parser_;
    std::unique_ptr<GLRenderHost> renderHost_;
    std::mutex player_mutex_;

    // Callbacks
    std::function<void(PlayerState)> on_state_changed_cb_;
    std::function<void(const std::string&)> on_error_cb_;
};