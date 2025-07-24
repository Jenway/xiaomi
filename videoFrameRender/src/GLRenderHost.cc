#include "GLRenderHost.hpp"
#include "EGLCore.hpp"
#include "Entitys.hpp"
#include "GLESRender.hpp"
#include <android/log.h>
#include <android/native_window.h>
#include <memory>
#include <optional>
#include <thread>

#define LOG_TAG "GLRenderHost"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace render_utils {

struct GLRenderHost::Impl {
    enum class State { IDLE,
        RUNNING,
        PAUSED,
        STOPPED };
    std::atomic<State> state_ { State::IDLE };

    std::thread render_thread_;
    std::mutex state_mutex_;
    std::condition_variable state_cond_;

    std::unique_ptr<EGLCore> egl_;
    std::unique_ptr<GLESRender> renderer_;
    SemQueue<std::shared_ptr<player_utils::VideoFrame>> frame_queue_ { 5 };

    std::shared_ptr<player_utils::VideoFrame> last_frame_rendered_;

    ANativeWindow* window_ {};
    bool initialized {};

    void renderLoop();
    void performDraw();
};

// --- 工厂函数和构造/析构函数 (无变化) ---
std::unique_ptr<GLRenderHost> GLRenderHost::create()
{
    return std::unique_ptr<GLRenderHost>(new GLRenderHost());
}
GLRenderHost::GLRenderHost()
    : impl_(std::make_unique<Impl>())
{
}
GLRenderHost::~GLRenderHost()
{
    release();
}

// --- 公共 API (init, release, pause, resume) (无变化) ---
bool GLRenderHost::init(ANativeWindow* window)
{
    if (impl_->state_ != Impl::State::IDLE) {
        LOGE("Cannot init, renderer is not in IDLE state.");
        return false;
    }
    LOGI("Initializing...");

    impl_->window_ = window;
    impl_->initialized = true;

    impl_->state_ = Impl::State::RUNNING;
    impl_->render_thread_ = std::thread(&Impl::renderLoop, impl_.get());

    LOGI("Initialized and render thread started.");
    return true;
}

void GLRenderHost::release()
{
    if (impl_->state_ == Impl::State::IDLE || impl_->state_ == Impl::State::STOPPED) {
        return;
    }
    LOGI("GLRenderHost::release() called. Setting state to STOPPED.");

    {
        std::lock_guard<std::mutex> lock(impl_->state_mutex_);
        impl_->state_ = Impl::State::STOPPED;
    }

    impl_->frame_queue_.shutdown();
    impl_->state_cond_.notify_one();

    if (impl_->render_thread_.joinable()) {
        LOGI("GLRenderHost::release() joining render thread...");
        impl_->render_thread_.join();
        LOGI("GLRenderHost::release() successfully joined render thread.");
    }

    LOGI("Render thread stopped.");
    impl_->initialized = false;
}

void GLRenderHost::pause()
{
    if (impl_->state_ != Impl::State::RUNNING)
        return;
    std::lock_guard<std::mutex> lock(impl_->state_mutex_);
    LOGI("Pausing render loop.");
    impl_->state_ = Impl::State::PAUSED;
    // 可以在这里触发一次重绘最后一帧，以确保暂停时屏幕内容正确
    // 但更优雅的方式是在 FSM 循环中处理
}

void GLRenderHost::resume()
{
    if (impl_->state_ != Impl::State::PAUSED)
        return;
    {
        std::lock_guard<std::mutex> lock(impl_->state_mutex_);
        LOGI("Resuming render loop.");
        impl_->state_ = Impl::State::RUNNING;
    }
    impl_->state_cond_.notify_one();
}

void GLRenderHost::submitFrame(std::shared_ptr<player_utils::VideoFrame> frame)
{
    if (!impl_ || !impl_->initialized || impl_->state_ == Impl::State::STOPPED) {
        return;
    }
    // LOGD("Frame submitted. PTS: %.3f", frame->pts); // 日志可以保留
    impl_->frame_queue_.push(std::move(frame));
}

// --- flush 方法简化 ---
void GLRenderHost::flush()
{
    if (!impl_)
        return;
    LOGI("Flushing frame queue.");
    impl_->frame_queue_.clear();

    // 重置最后一帧的引用，因为 seek 后它已失效
    impl_->last_frame_rendered_.reset();
}

// -----------------------------------------------------------------------------
//  Private Impl methods (The Core of the FSM)
// -----------------------------------------------------------------------------

// --- renderLoop 保持 FSM 结构，但行为更简单 ---
void GLRenderHost::Impl::renderLoop()
{
    LOGI(">>> Render thread entered.");

    egl_ = std::make_unique<EGLCore>();
    if (!egl_->init(window_)) {
        LOGE("EGL initialization failed inside render thread.");
        state_ = State::STOPPED;
        return;
    }

    auto render_opt = GLESRender::create();
    if (!render_opt) {
        LOGE("GLESRender creation failed inside render thread.");
        state_ = State::STOPPED;
        return;
    }
    renderer_ = std::move(*render_opt);
    auto [width, height] = egl_->querySurfaceSize();
    renderer_->on_viewport_change(width, height);

    LOGI("EGL and Renderer initialized successfully on render thread.");

    while (state_ != State::STOPPED) {
        {
            std::unique_lock<std::mutex> lock(state_mutex_);
            state_cond_.wait(lock, [this] {
                // 当处于 PAUSED 状态时，线程将在此处阻塞
                // 只有在 RUNNING 或 STOPPED 时才会继续
                return state_ == State::RUNNING || state_ == State::STOPPED;
            });

            if (state_ == State::STOPPED) {
                LOGI("Exit signal received in render loop.");
                break;
            }
        }

        // 渲染循环的核心现在只调用 performDraw
        performDraw();
    }

    LOGI("<<< Render thread is exiting. Releasing EGL.");
    renderer_.reset();
    if (egl_) {
        egl_->release();
        egl_.reset();
    }
}

// --- 【关键修改】performDraw 被极大简化 ---
void GLRenderHost::Impl::performDraw()
{
    std::shared_ptr<VideoFrame> frame_to_render;

    // 使用阻塞式 pop。它会一直等待，直到有帧可供渲染或队列被关闭。
    // 这消除了之前基于超时的复杂重绘逻辑。
    bool has_frame = frame_queue_.wait_and_pop(frame_to_render);

    if (!has_frame) {
        // wait_and_pop 返回 false 意味着队列被关闭 (shutdown)
        // 渲染线程即将退出其主循环，所以这里什么都不用做。
        return;
    }

    // 只要拿到帧，就立即渲染
    if (renderer_ && frame_to_render) {
        renderer_->paint(frame_to_render);
        egl_->swapBuffers();

        // 保存这一帧的引用，以便在暂停时可以重绘
        last_frame_rendered_ = std::move(frame_to_render);
    }
}

} // namespace render_utils