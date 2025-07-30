#pragma once

#include "Entitys.hpp"
#include <functional>
#include <jni.h>
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
    double getDuration() const;
    player_utils::PlayerState getState() const;
    double getPosition() const;
    void setSpeed(float speed);

    void setJniEnv(JavaVM* vm, jobject player_object);
    void setOnStateChangedCallback(std::function<void(player_utils::PlayerState)> cb);
    void setOnErrorCallback(std::function<void(const std::string&)> cb);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};