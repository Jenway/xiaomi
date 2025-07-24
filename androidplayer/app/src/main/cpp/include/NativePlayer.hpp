#pragma once

#include "Entitys.hpp" // 包含 PlayerState 定义
#include <functional>
#include <memory>
#include <string>

struct ANativeWindow;

class NativePlayer : public std::enable_shared_from_this<NativePlayer> {
public:
    NativePlayer();
    ~NativePlayer();

    void play(const std::string& path, ANativeWindow* window);
    void pause(bool is_paused);
    void stop();
    void seek(double time_sec);

    void setOnStateChangedCallback(std::function<void(player_utils::PlayerState)> cb);
    void setOnErrorCallback(std::function<void(const std::string&)> cb);

    NativePlayer(const NativePlayer&) = delete;
    NativePlayer& operator=(const NativePlayer&) = delete;

private:
    // 指向所有内部实现的指针
    struct Impl;
    std::unique_ptr<Impl> impl_;
};