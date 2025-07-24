#pragma once
#include <EGL/egl.h>
#include <utility>
namespace render_utils {

constexpr EGLint ATTRIB_LIST[] = {
    EGL_BLUE_SIZE, 8,
    EGL_GREEN_SIZE, 8,
    EGL_RED_SIZE, 8,
    EGL_ALPHA_SIZE, 8, // 确保有alpha通道
    EGL_DEPTH_SIZE, 0, // 无深度缓冲
    EGL_STENCIL_SIZE, 0, // 无模板缓冲
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_NONE
};

constexpr EGLint CONTEXT_ATTRIB_LIST[] = {
    EGL_CONTEXT_CLIENT_VERSION, 3,
    EGL_NONE
};

class EGLCore {
public:
    bool init(ANativeWindow* native_window);
    void swapBuffers();
    void release();
    bool makeCurrent();
    void doneCurrent();
    std::pair<int32_t, int32_t> querySurfaceSize();

private:
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
    EGLConfig config_ {};

    bool initDisplay();
    bool chooseConfig();
    bool createContext();
    bool createSurface(ANativeWindow* window);
};

} // namespace render_utils
