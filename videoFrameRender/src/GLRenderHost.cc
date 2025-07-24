#include "GLRenderHost.hpp"
#include "EGLCore.hpp"
#include "Entitys.hpp"
#include "GLESRender.hpp"
#include <android/native_window.h>
#include <memory>
#include <optional>
#include <thread>
// 新增: 引入 Android 日志头文件
#include <android/log.h>

#define LOG_TAG "GLRenderHost"
// 使用 LOGD 进行调试，方便在 release 版本中过滤掉
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace render_utils {
struct GLRenderHost::Impl {
    // --- 状态机 ---
    enum class State { IDLE,
        RUNNING,
        PAUSED,
        STOPPED };
    std::atomic<State> state_ { State::IDLE }; // 使用原子类型简化多线程访问

    // --- 渲染线程和同步 ---
    std::thread render_thread_;
    std::mutex state_mutex_; // 用于保护状态转换和条件变量
    std::condition_variable state_cond_;

    // --- core ---
    std::unique_ptr<EGLCore> egl_;
    std::unique_ptr<GLESRender> renderer_;
    SemQueue<std::shared_ptr<player_utils::VideoFrame>> frame_queue_ { 5 }; // 假设队列大小为5

    // 同步相关的时钟变量
    std::chrono::steady_clock::time_point start_time_ = std::chrono::steady_clock::time_point::min();
    double last_rendered_pts_ = -1.0;
    std::shared_ptr<player_utils::VideoFrame> last_frame_rendered_; // 用于重绘

    ANativeWindow* window_ {};
    bool initialized {};
    // --- 内部方法 ---
    void renderLoop(); // 渲染线程的入口
    void performDraw(); // 封装了旧的 drawFrame 逻辑
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

    // 启动渲染线程，并切换状态
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

    // 1. 设置停止状态
    {
        std::lock_guard<std::mutex> lock(impl_->state_mutex_);
        impl_->state_ = Impl::State::STOPPED;
    }

    // 2. 关闭帧队列，唤醒可能在等待的 submitFrame 和 renderLoop
    impl_->frame_queue_.shutdown();

    // 3. 唤醒渲染线程，让它检查到 STOPPED 状态并退出
    impl_->state_cond_.notify_one();

    LOGI("GLRenderHost::release() notified the condition variable.");

    // 4. 等待渲染线程完全结束
    if (impl_->render_thread_.joinable()) {
        LOGI("GLRenderHost::release() is about to join render thread [ID: %d]...", impl_->render_thread_.get_id());
        impl_->render_thread_.join();
        LOGI("GLRenderHost::release() successfully joined render thread.");
    }

    LOGI("Render thread stopped. Cleaning up EGL.");

    impl_->initialized = false;
}

void GLRenderHost::pause()
{
    if (impl_->state_ != Impl::State::RUNNING)
        return;
    std::lock_guard<std::mutex> lock(impl_->state_mutex_);
    LOGI("Pausing render loop.");
    impl_->state_ = Impl::State::PAUSED;
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
    // 唤醒可能因暂停而等待的线程
    impl_->state_cond_.notify_one();
}

void GLRenderHost::submitFrame(std::shared_ptr<player_utils::VideoFrame> frame)
{
    if (!impl_ || !impl_->initialized)
        return;
    if (impl_->state_ == Impl::State::STOPPED)
        return;

    LOGD("Frame submitted. PTS: %.3f", frame->pts);
    impl_->frame_queue_.push(std::move(frame));
}

void GLRenderHost::flush()
{
    if (!impl_)
        return;
    LOGI("Flushing frame queue and resetting clock.");
    impl_->frame_queue_.clear();

    // 重置时钟和状态
    impl_->start_time_ = std::chrono::steady_clock::time_point::min();
    impl_->last_frame_rendered_.reset();
    impl_->last_rendered_pts_ = -1.0;
}

// -----------------------------------------------------------------------------
//  Private Impl methods (The Core of the FSM)
// -----------------------------------------------------------------------------

void GLRenderHost::Impl::renderLoop()
{
    LOGI(">>> Render thread [ID: %d] entered.", std::this_thread::get_id());

    // 为什么不在 init 中调用？一个 EGL 上下文在同一时间只能被一个线程持有
    egl_ = std::make_unique<EGLCore>();

    if (!egl_->init(window_)) {
        LOGE("EGL initialization failed inside render thread.");
        state_ = State::STOPPED; // 标记为错误状态并退出
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
            // 2. FSM 状态检查与等待
            std::unique_lock<std::mutex> lock(state_mutex_);

            LOGI("Render thread [ID: %d] is about to wait. State: %d", std::this_thread::get_id(), static_cast<int>(state_.load()));

            state_cond_.wait(lock, [this] {
                // 仅在 RUNNING 或 STOPPED 状态下继续，PAUSED 时会阻塞
                return state_ == State::RUNNING || state_ == State::STOPPED;
            });

            LOGI("Render thread [ID: %d] woke up. State: %d", std::this_thread::get_id(), static_cast<int>(state_.load()));

            // 3. 检查是否需要退出循环
            if (state_ == State::STOPPED) {
                LOGI("Exit signal received in render loop.");
                break;
            }
        }

        // 4. 执行单帧渲染逻辑
        performDraw();

        // (可选) 为了避免 CPU 忙等，可以在这里加一个小的休眠
        // 这在没有帧可渲染时特别有用。
        // 但由于我们的 performDraw 有阻塞逻辑，所以可能不是必需的。
        // std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    // 5. 退出循环后，解绑并清理 EGL 上下文
    LOGI("<<< Render thread [ID: %d] is exiting. Releasing EGL.", std::this_thread::get_id());
    renderer_.reset();
    if (egl_) {
        egl_->release(); // 确保 EGL 资源在同一个线程被释放
        egl_.reset();
    }
}

void GLRenderHost::Impl::performDraw()
{
    // 这里的逻辑就是你原来 drawFrame 的逻辑，稍作调整
    // 使用带超时的 wait_and_pop 是一个很好的实践，可以避免在没有新帧时无限期阻塞
    std::shared_ptr<VideoFrame> frame_to_render;
    bool has_frame = frame_queue_.wait_and_pop(frame_to_render, std::chrono::milliseconds(10));

    if (!has_frame) {
        // 超时了，队列为空。重绘上一帧以避免黑屏
        if (last_frame_rendered_ && state_ == State::RUNNING) {
            renderer_->paint(last_frame_rendered_);
            egl_->swapBuffers();
        }
        return;
    }

    if (start_time_ == std::chrono::steady_clock::time_point::min()) {
        start_time_ = std::chrono::steady_clock::now();
        LOGI("First frame. Clock started. PTS: %.3f", frame_to_render->pts);
    }

    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = now - start_time_;
    double frame_pts = frame_to_render->pts;
    double time_to_wait = frame_pts - elapsed_seconds.count();

    if (time_to_wait > 0.001) { // 延迟大于1ms才等待
        std::this_thread::sleep_for(std::chrono::duration<double>(time_to_wait));
    }

    // 渲染帧
    renderer_->paint(frame_to_render);
    egl_->swapBuffers();

    last_rendered_pts_ = frame_to_render->pts;
    last_frame_rendered_ = std::move(frame_to_render);

    // 丢帧逻辑（简化版）：如果队列中还有帧，并且它的时间戳也已经过去，就丢掉它
    while (auto next_frame_opt = frame_queue_.front()) {
        if ((*next_frame_opt)->pts <= elapsed_seconds.count()) {
            std::shared_ptr<player_utils::VideoFrame> dropped_frame;
            frame_queue_.try_pop(dropped_frame);
            LOGI("Dropping late frame. PTS: %.3f", dropped_frame->pts);
        } else {
            break;
        }
    }
}

} // namespace render_utils