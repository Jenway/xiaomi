#include "GLRenderHost.hpp"
#include "EGLCore.hpp"
#include "Entitys.hpp"
#include "GLESRender.hpp"
#include <android/native_window.h>
#include <memory>
#include <optional>

// 新增: 引入 Android 日志头文件
#include <android/log.h>

#define LOG_TAG "GLRenderHost"
// 使用 LOGD 进行调试，方便在 release 版本中过滤掉
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace render_utils {
std::unique_ptr<GLRenderHost> GLRenderHost::create()
{
    auto instance = std::unique_ptr<GLRenderHost>(new GLRenderHost());
    // 这行代码在 init 中也会执行，可以考虑只保留一个地方
    // instance->impl_ = std::make_unique<Impl>();
    return instance;
}

struct GLRenderHost::Impl {
    std::unique_ptr<EGLCore> egl_;
    std::unique_ptr<GLESRender> renderer_;

    // 使用你之前定义的 SemQueue
    SemQueue<std::shared_ptr<player_utils::VideoFrame>> frame_queue_ { 5 }; // 假设队列大小为5

    bool initialized = false;

    // 同步相关的时钟变量
    std::chrono::steady_clock::time_point start_time_ = std::chrono::steady_clock::time_point::min();
    double last_rendered_pts_ = -1.0;
    std::shared_ptr<player_utils::VideoFrame> last_frame_rendered_; // 用于重绘
};

// 构造函数中初始化 Impl
GLRenderHost::GLRenderHost()
    : impl_(std::make_unique<Impl>())
{
}

GLRenderHost::~GLRenderHost()
{
    // 确保在析构时调用 release
    release();
}

bool GLRenderHost::init(ANativeWindow* window)
{
    LOGI("Initializing...");

    impl_->egl_ = std::make_unique<EGLCore>();
    if (!impl_->egl_->init(window)) {
        LOGE("EGLCore initialization failed.");
        return false;
    }

    // 假设 GLESRender::create() 返回一个 std::optional<std::unique_ptr<GLESRender>>
    auto render_opt = GLESRender::create();
    if (!render_opt) {
        LOGE("GLESRender creation failed.");
        return false;
    }
    impl_->renderer_ = std::move(*render_opt);

    auto [width, height] = impl_->egl_->querySurfaceSize();
    impl_->renderer_->on_viewport_change(width, height);

    impl_->initialized = true;
    LOGI("Initialized successfully. Viewport: %dx%d", width, height);
    return true;
}

void GLRenderHost::release()
{
    if (!impl_ || !impl_->initialized) {
        return;
    }
    LOGI("Releasing resources...");

    impl_->initialized = false;
    impl_->frame_queue_.shutdown(); // 关闭队列，唤醒所有等待者

    // 释放顺序很重要：先释放依赖 EGL context 的 renderer
    if (impl_->renderer_) {
        impl_->renderer_.reset();
    }

    if (impl_->egl_) {
        impl_->egl_->release();
        impl_->egl_.reset();
    }
    LOGI("Resources released.");
}

void GLRenderHost::submitFrame(std::shared_ptr<player_utils::VideoFrame> frame)
{
    if (!impl_ || !impl_->initialized) {
        return;
    }
    LOGD("Frame submitted. PTS: %.3f", frame->pts);
    impl_->frame_queue_.push(std::move(frame));
}

void GLRenderHost::drawFrame()
{
    if (!impl_ || !impl_->initialized) {
        return;
    }

    // “窥视”队列的头部帧，但不弹出
    auto frame_opt = impl_->frame_queue_.front();

    if (!frame_opt) {
        LOGD("Draw call skipped: Frame queue is empty. Re-rendering last frame.");
        // 如果队列为空，可以重绘上一帧以避免黑屏
        if (impl_->last_frame_rendered_) {
            impl_->egl_->makeCurrent();
            impl_->renderer_->paint(impl_->last_frame_rendered_);
            impl_->egl_->swapBuffers();
        }
        return;
    }

    auto& frame_to_show = *frame_opt;

    // 如果是第一帧，则记录当前时间为播放开始时间
    if (impl_->start_time_ == std::chrono::steady_clock::time_point::min()) {
        impl_->start_time_ = std::chrono::steady_clock::now();
        LOGI("First frame received. Clock started. First PTS: %.3f", frame_to_show->pts);
    }

    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = now - impl_->start_time_;
    double frame_pts = frame_to_show->pts;

    // 计算延迟或需要等待的时间
    double time_diff = elapsed_seconds.count() - frame_pts;

    if (time_diff >= 0) {
        // --- 渲染时间已到或已过 ---
        LOGD("Render Now. PTS: %.3f, Elapsed: %.3f, Delay: %.0f ms",
            frame_pts, elapsed_seconds.count(), time_diff * 1000.0);

        // 弹出这一帧
        std::shared_ptr<player_utils::VideoFrame> frame_to_render;
        impl_->frame_queue_.wait_and_pop(frame_to_render); // 这里会成功，因为我们已确认有帧

        // 渲染
        impl_->egl_->makeCurrent();
        impl_->renderer_->paint(frame_to_render);
        impl_->egl_->swapBuffers();

        // 缓存刚刚渲染的帧
        impl_->last_rendered_pts_ = frame_to_render->pts;
        impl_->last_frame_rendered_ = std::move(frame_to_render);

        // (优化) 检查并丢弃其他也已经迟到的帧
        while (auto next_frame_opt = impl_->frame_queue_.front()) {
            if ((*next_frame_opt)->pts <= elapsed_seconds.count()) {
                std::shared_ptr<player_utils::VideoFrame> dropped_frame;
                impl_->frame_queue_.try_pop(dropped_frame);
                LOGI("Dropping late frame. PTS: %.3f, Elapsed: %.3f", dropped_frame->pts, elapsed_seconds.count());
            } else {
                break; // 下一帧时间未到，停止丢弃
            }
        }

    } else {
        // --- 渲染时间未到 ---
        LOGD("Waiting.     PTS: %.3f, Elapsed: %.3f, Wait for: %.0f ms",
            frame_pts, elapsed_seconds.count(), -time_diff * 1000.0);

        // 重绘上一帧以填充等待时间
        if (impl_->last_frame_rendered_) {
            impl_->egl_->makeCurrent();
            impl_->renderer_->paint(impl_->last_frame_rendered_);
            impl_->egl_->swapBuffers();
        }
    }
}

// ... seek 和 pause 的逻辑也需要修改时钟 ...
void GLRenderHost::flush()
{
    if (!impl_)
        return;
    LOGI("Seek requested. Clearing frame queue and resetting clock.");
    impl_->frame_queue_.clear();
    impl_->start_time_ = std::chrono::steady_clock::time_point::min();
    impl_->last_frame_rendered_.reset();
    impl_->last_rendered_pts_ = -1.0;
}

} // namespace render_utils