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

static void log_thread_exit()
{
    std::stringstream ss;
    ss << "<<< Render thread [ID: " << std::this_thread::get_id() << "] is exiting.";
    LOGI("%s", ss.str().c_str());
}

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

    LOGI("Initialized and render thread started.");
    return true;
}

void GLRenderHost::start()
{
    impl_->render_thread_ = std::thread(&Impl::renderLoop, impl_.get());
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
    // LOGD("Frame submitted. PTS: %.3f", frame->pts);
    impl_->frame_queue_.push(std::move(frame));
}

void GLRenderHost::flush()
{
    if (!impl_)
        return;
    LOGI("Flushing frame queue.");
    impl_->frame_queue_.clear();

    impl_->last_frame_rendered_.reset();
}

// --- FSM ---
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
                return state_ == State::RUNNING || state_ == State::STOPPED;
            });

            if (state_ == State::STOPPED) {
                LOGI("Exit signal received in render loop.");
                break;
            }
        }
        performDraw();
    }

    LOGI("<<< Render thread is exiting. Releasing EGL.");
    log_thread_exit();
    renderer_.reset();
    if (egl_) {
        // LOGI("EGL releasing on thread %d...", std::this_thread::get_id());
        egl_->release();
        // LOGI("EGL released on thread %d.", std::this_thread::get_id());
        egl_.reset();
    }
}

void GLRenderHost::Impl::performDraw()
{
    std::shared_ptr<VideoFrame> frame_to_render;

    bool has_frame = frame_queue_.wait_and_pop(frame_to_render);

    if (!has_frame) {
        return;
    }

    if (renderer_ && frame_to_render) {
        renderer_->paint(frame_to_render);
        egl_->swapBuffers();

        last_frame_rendered_ = std::move(frame_to_render);
    }
}

} // namespace render_utils