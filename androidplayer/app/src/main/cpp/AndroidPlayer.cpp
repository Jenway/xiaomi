//
// Created by Jenway on 2025/7/22.
//
#include <jni.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <memory>
#include <mutex>
#include <string>
#include "Mp4Parser.hpp"

#define LOG_TAG "PlayerJNI"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

using namespace mp4parser;

namespace {
    std::unique_ptr<Mp4Parser> parser;
    std::mutex parser_mutex;

    // 保留 JavaVM 和回调
    JavaVM* g_vm = nullptr;
    jobject g_surface = nullptr;
    jobject g_player_instance = nullptr;
}

extern "C" jint JNI_OnLoad(JavaVM* vm, void*) {
    g_vm = vm;
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativePlay(JNIEnv* env, jobject thiz, jstring path, jobject surface) {
    const char* file_path = env->GetStringUTFChars(path, nullptr);
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
        LOGE("Failed to get ANativeWindow from Surface");
        return -1;
    }
    Config config;
    config.file_path = file_path;

    Callbacks callbacks;
    callbacks.on_frame_decoded = [window](const AVFrame* frame) {
        std::string desc = describe_frame_info(frame);
        LOGI("Decoded frame info: %s", desc.c_str());
    };


    callbacks.on_state_changed = [](PlayerState state) {
        // TODO: 如果需要，反调到 Java，通知状态变化
    };
    callbacks.on_error = [](const std::string& msg) {
        LOGE("Native error: %s", msg.c_str());
    };

    std::lock_guard<std::mutex> lock(parser_mutex);
    parser = Mp4Parser::create(config, callbacks);
    parser->start();

    env->ReleaseStringUTFChars(path, file_path);
    return 0;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_androidplayer_Player_nativePause(JNIEnv*, jobject, jboolean pause) {
    std::lock_guard<std::mutex> lock(parser_mutex);
    if (!parser) return;
    if (pause) parser->pause();
    else parser->resume();
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativeStop(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(parser_mutex);
    if (parser) {
        parser->stop();
        parser.reset();
    }
    return 0;
}

// 下面是模拟实现。注意：你需要在 Mp4Parser 实现中添加这些接口（如 getPosition）
extern "C" JNIEXPORT jdouble JNICALL
Java_com_example_androidplayer_Player_nativeGetPosition(JNIEnv*, jobject) {
    return 0.0; // TODO: 替换为 parser->get_position()
}

extern "C" JNIEXPORT jdouble JNICALL
Java_com_example_androidplayer_Player_nativeGetDuration(JNIEnv*, jobject) {
    return 100.0; // TODO: 替换为 parser->get_duration()
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativeSeek(JNIEnv*, jobject, jdouble position) {
    // TODO: 替换为 parser->seek(position)
    return 0;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativeSetSpeed(JNIEnv*, jobject, jfloat speed) {
    // TODO: 替换为 parser->set_speed(speed)
    return 0;
}
