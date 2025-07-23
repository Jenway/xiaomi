#pragma once

// For logging (Android's standard logging)
#include <android/log.h>

#define LOG_TAG "EGLCore"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Helper macro for EGL error checking
#define CHECK_EGL_ERROR(msg)                                                   \
    do {                                                                       \
        EGLint err = eglGetError();                                            \
        if (err != EGL_SUCCESS) {                                              \
            LOGE("%s: EGL error 0x%x at %s:%d", msg, err, __FILE__, __LINE__); \
        }                                                                      \
    } while (0)