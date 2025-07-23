#include "GLRenderHost.hpp"
#include "EGLCore.hpp"
#include "GLESRender.hpp"
#include <android/native_window.h>
#include <android/native_window_jni.h> // 如果你从 Java 层传 surface
#include <memory>
#include <optional>

namespace render_utils {
std::unique_ptr<GLRenderHost> GLRenderHost::create()
{
    auto instance = std::unique_ptr<GLRenderHost>(new GLRenderHost());
    instance->impl_ = std::make_unique<Impl>();
    return instance;
}

struct GLRenderHost::Impl {
    std::unique_ptr<EGLCore> egl_;
    std::unique_ptr<GLESRender> renderer_;
    static constexpr int kRingBufferSize = 5;
    SemQueue<std::shared_ptr<VideoFrame>, RingBuffer<std::shared_ptr<VideoFrame>, kRingBufferSize>> ringBuffer { kRingBufferSize };
    bool initialized = false;
};

GLRenderHost::~GLRenderHost() = default;

bool GLRenderHost::init(ANativeWindow* window)
{
    impl_ = std::make_unique<Impl>();

    impl_->egl_ = std::make_unique<EGLCore>();
    if (!impl_->egl_->init(window)) {
        return false;
    }

    auto render_opt = GLESRender::create();
    if (!render_opt) {
        return false;
    }
    impl_->renderer_ = std::move(*render_opt);
    auto [width, height] = impl_->egl_->querySurfaceSize();

    impl_->renderer_->on_viewport_change(width, height);
    impl_->initialized = true;
    return true;
}

void GLRenderHost::release()
{
    if (!impl_) {
        return;
    }

    if (impl_->renderer_) {
        impl_->renderer_.reset();
    }

    if (impl_->egl_) {
        impl_->egl_->release();
        impl_->egl_.reset();
    }

    impl_.reset();
}

void GLRenderHost::submitFrame(std::shared_ptr<VideoFrame> frame)
{
    if (!impl_ || !impl_->initialized) {
        return;
    }

    impl_->ringBuffer.push(std::move(frame));
}

void GLRenderHost::drawFrame()
{
    if (!impl_ || !impl_->initialized) {
        return;
    }

    std::shared_ptr<VideoFrame> frame;
    impl_->ringBuffer.wait_and_pop(frame);
    impl_->egl_->makeCurrent();
    impl_->renderer_->paint(frame);
    impl_->egl_->swapBuffers();
}

} // namespace render_utils
