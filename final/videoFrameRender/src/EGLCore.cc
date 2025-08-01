#include "EGLCore.hpp"
#include <EGL/egl.h>
#include <android/native_window.h>
#include <cstdint>
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

namespace render_utils {
bool EGLCore::init(ANativeWindow* native_window)
{
    if (native_window == nullptr) {
        LOGE("EGLCore::init received null window");
        return false;
    }
    // 保存并获取窗口的所有权
    window_ = native_window;
    ANativeWindow_acquire(window_);
    LOGI("ANativeWindow acquired.");
    if (!initDisplay())
        return false;
    if (!chooseConfig())
        return false;
    if (!createContext())
        return false;
    if (!createSurface())
        return false;
    // makeCurrent 后，OpenGL 的命令就可以同步到 EGL Surface 上了
    if (!makeCurrent())
        return false;
    return true;
}

bool EGLCore::initDisplay()
{
    display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display_ == EGL_NO_DISPLAY) {
        LOGE("Failed to get EGL display.");
        return false;
    }

    EGLint major = 0;
    EGLint minor = 0;
    if (eglInitialize(display_, &major, &minor) == 0U) {
        LOGE("Failed to initialize EGL. Error: 0x%x", eglGetError());
        display_ = EGL_NO_DISPLAY;
        return false;
    }

    LOGI("EGL initialized. Version: %d.%d", major, minor);
    return true;
}

bool EGLCore::chooseConfig()
{
    EGLint num_configs = 0;
    if ((eglChooseConfig(display_, ATTRIB_LIST, &config_, 1, &num_configs) == 0U) || num_configs == 0) {
        LOGE("Failed to choose EGL config. Error: 0x%x", eglGetError());
        release();
        return false;
    }

    LOGI("EGL config chosen successfully.");
    return true;
}

bool EGLCore::createContext()
{
    context_ = eglCreateContext(display_, config_, EGL_NO_CONTEXT, CONTEXT_ATTRIB_LIST);
    if (context_ == EGL_NO_CONTEXT) {
        LOGE("Failed to create EGL context. Error: 0x%x", eglGetError());
        release();
        return false;
    }

    LOGI("EGL context created successfully.");
    return true;
}

bool EGLCore::createSurface()
{
    if (window_ == nullptr) {
        LOGE("EGLCore::createSurface received null window");
        return false;
    }

    surface_ = eglCreateWindowSurface(display_, config_, window_, nullptr);
    if (surface_ == EGL_NO_SURFACE) {
        LOGE("Failed to create EGL window surface. Error: 0x%x", eglGetError());
        release();
        return false;
    }

    LOGI("EGL surface created successfully.");
    return true;
}

bool EGLCore::makeCurrent()
{

    if (eglMakeCurrent(display_, surface_, surface_, context_) == 0U) {
        LOGE("Failed to make EGL context current. Error: 0x%x", eglGetError());
        return false;
    }

    return true;
}
void EGLCore::doneCurrent()
{
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
}
void EGLCore::swapBuffers()
{
    if (display_ == EGL_NO_DISPLAY || surface_ == EGL_NO_SURFACE) {
        LOGW("Attempted to swap buffers with uninitialized EGLCore.");
        return;
    }

    if (eglSwapBuffers(display_, surface_) == 0U) {
        EGLint err = eglGetError();
        if (err == EGL_BAD_SURFACE || err == EGL_CONTEXT_LOST) {
            LOGE("EGL surface lost or context lost. Error: 0x%x", err);
        } else {
            LOGE("Failed to swap EGL buffers. Error: 0x%x", err);
        }
    }
}

void EGLCore::release()
{
    if (display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(display_, context_);
            context_ = EGL_NO_CONTEXT;
            LOGI("EGL context destroyed.");
        }
        if (surface_ != EGL_NO_SURFACE) {
            eglDestroySurface(display_, surface_);
            surface_ = EGL_NO_SURFACE;
            LOGI("EGL surface destroyed.");
        }
        if (window_ != nullptr) {
            ANativeWindow_release(window_);
            window_ = nullptr;
            LOGI("ANativeWindow released.");
        }
        eglTerminate(display_);
        display_ = EGL_NO_DISPLAY;
        LOGI("EGL display terminated.");
    }
}

std::pair<int32_t, int32_t> EGLCore::querySurfaceSize()
{
    int width_ = 0;
    int height_ = 0;
    eglQuerySurface(display_, surface_, EGL_WIDTH, &width_);
    eglQuerySurface(display_, surface_, EGL_HEIGHT, &height_);
    LOGI("EGL surface dimensions: %dx%d", width_, height_);
    return { width_, height_ };
}

} // namespace render_utils