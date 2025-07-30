#pragma once

#include "Entitys.hpp"
#include <aaudio/AAudio.h>
#include <android/log.h>
#include <android/native_window.h>
#include <jni.h>

class JniCallbackHandler {
public:
    JniCallbackHandler(JavaVM* vm, jobject player_object);
    ~JniCallbackHandler();
    JniCallbackHandler(const JniCallbackHandler&) = delete;
    JniCallbackHandler& operator=(const JniCallbackHandler&) = delete;

    void notifyStateChanged(player_utils::PlayerState newState);

private:
    JavaVM* jvm_;
    jobject jni_player_object_;
    jmethodID on_state_changed_mid_ = nullptr;
};