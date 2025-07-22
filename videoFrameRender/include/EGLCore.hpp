class EGLCore {
public:
    bool init(void* native_window);
    void makeCurrent();
    void swapBuffers();
    void release();
private:
    EGLDisplay display_ = EGL_NO_DISPLAY;
    EGLSurface surface_ = EGL_NO_SURFACE;
    EGLContext context_ = EGL_NO_CONTEXT;
    // ...
    // 可以持有 OpenGLWrapper* 作为成员
};
