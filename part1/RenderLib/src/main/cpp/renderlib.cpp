#include <jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/bitmap.h>
#include "glDisplayer.hpp"
#include "glBitmapRender.hpp"


#ifdef ANDROID
#include <android/log.h>
#define MITAG "MI"
#define MILOG(level, format, ...) \
    __android_log_print(level, MITAG, "[%s:%d] " format, __FILE__, __LINE__, ##__VA_ARGS__)

#define MILOGV(...) MILOG(ANDROID_LOG_VERBOSE, __VA_ARGS__)
#define MILOGD(...) MILOG(ANDROID_LOG_DEBUG, __VA_ARGS__)
#define MILOGI(...) MILOG(ANDROID_LOG_INFO, __VA_ARGS__)
#define MILOGW(...) MILOG(ANDROID_LOG_WARN, __VA_ARGS__)
#define MILOGE(...) MILOG(ANDROID_LOG_ERROR, __VA_ARGS__)
#else
#define MILOGV(...) printf(__VA_ARGS__)
#define MILOGD(...) printf(__VA_ARGS__)
#define MILOGI(...) printf(__VA_ARGS__)
#define MILOGW(...) printf(__VA_ARGS__)
#define MILOGE(...) printf(__VA_ARGS__)
#endif

const char* eglErrorStr(EGLint error) {
    switch (error) {
        case EGL_SUCCESS: return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED: return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS: return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC: return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE: return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONFIG: return "EGL_BAD_CONFIG";
        case EGL_BAD_CONTEXT: return "EGL_BAD_CONTEXT";
        case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY: return "EGL_BAD_DISPLAY";
        case EGL_BAD_MATCH: return "EGL_BAD_MATCH";
        case EGL_BAD_NATIVE_PIXMAP: return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW: return "EGL_BAD_NATIVE_WINDOW";
        case EGL_BAD_PARAMETER: return "EGL_BAD_PARAMETER";
        case EGL_BAD_SURFACE: return "EGL_BAD_SURFACE";
        case EGL_CONTEXT_LOST: return "EGL_CONTEXT_LOST";
        default: return "Unknown EGL error";
    }
}

// 执行完gl命令后，可以用这个宏定义检查gl命令执行完是否有错误
#define CHECK_GL() eglErrorStr(eglGetError())

/**
 * 实现两张图片在android surfaceview上的显示
 */
extern "C"
JNIEXPORT void JNICALL
Java_com_mi_renderlib_RenderActivity_render(JNIEnv *env, jobject thiz,
                                            jobject bgBitmap,
                                            jobject fgBitmap,
                                            jobject surface) {
    // 1. 获取ANativeWindow
    ANativeWindow *window = ANativeWindow_fromSurface(env, surface);
    if (!window) {
        MILOGE("Failed to get ANativeWindow");
        return;
    }

    // 2. 初始化EGL
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        MILOGE("eglGetDisplay failed: %s", CHECK_GL());
        ANativeWindow_release(window);
        return;
    }

    if (!eglInitialize(display, nullptr, nullptr)) {
        MILOGE("eglInitialize failed: %s", CHECK_GL());
        ANativeWindow_release(window);
        return;
    }

    // 3. 选择EGL配置
    const EGLint configAttribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_BLUE_SIZE, 8,
            EGL_GREEN_SIZE, 8,
            EGL_RED_SIZE, 8,
            EGL_ALPHA_SIZE, 8,  // 确保有alpha通道
            EGL_DEPTH_SIZE, 0,  // 无深度缓冲
            EGL_STENCIL_SIZE, 0,// 无模板缓冲
            EGL_NONE
    };

    EGLConfig config;
    EGLint numConfigs;
    if (!eglChooseConfig(display, configAttribs, &config, 1, &numConfigs) || numConfigs == 0) {
        MILOGE("eglChooseConfig failed: %s", eglErrorStr(eglGetError()));
        ANativeWindow_release(window);
        return;
    }

    // 4. 创建EGL上下文
    const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT) {
        MILOGE("eglCreateContext failed: %s", eglErrorStr(eglGetError()));
        ANativeWindow_release(window);
        return;
    }

    // 5. 创建EGLSurface
    EGLSurface eglSurface = eglCreateWindowSurface(display, config, window, nullptr);
    if (eglSurface == EGL_NO_SURFACE) {
        MILOGE("eglCreateWindowSurface failed: %s", eglErrorStr(eglGetError()));
        eglDestroyContext(display, context);
        ANativeWindow_release(window);
        return;
    }

    // 6. 设置当前上下文
    if (!eglMakeCurrent(display, eglSurface, eglSurface, context)) {
        MILOGE("eglMakeCurrent failed: %s", eglErrorStr(eglGetError()));
        eglDestroySurface(display, eglSurface);
        eglDestroyContext(display, context);
        ANativeWindow_release(window);
        return;
    }

    // 7. 设置交换间隔
    eglSwapInterval(display, 0);  // 0 = 不等待垂直同步

    // 8. 获取窗口尺寸并设置视口
    int32_t width = ANativeWindow_getWidth(window);
    int32_t height = ANativeWindow_getHeight(window);

    if (width > 0 && height > 0) {
        MILOGD("Setting viewport: %d x %d", width, height);
        glViewport(0, 0, width, height);
    } else {
        MILOGE("Invalid window dimensions: %d x %d", width, height);
        glViewport(0, 0, 1024, 768); // 安全回退值
    }

    // 11. 加载位图数据
    auto getImgDesc = [&env](jobject bitmap){
        glDisplayer::ImageDesc desc{};
        AndroidBitmapInfo info;
        if (AndroidBitmap_getInfo(env, bitmap, &info) < 0) {
            MILOGE("Failed to get AndroidBitmapInfo for bitmap!");
            return desc;
        }
        desc.imgWidth = info.width;
        desc.imgHeight = info.height;
        return desc;
    };

    auto fgDesc = getImgDesc(fgBitmap);
    auto bgDesc = getImgDesc(bgBitmap);

// 锁定像素
    void* fgPixels = nullptr;
    void* bgPixels = nullptr;
    if (AndroidBitmap_lockPixels(env, fgBitmap, &fgPixels) < 0 ||
        AndroidBitmap_lockPixels(env, bgBitmap, &bgPixels) < 0) {
        // 锁定失败，处理
        return;
    }

    fgDesc.data = fgPixels;
    bgDesc.data = bgPixels;

//====================== 设置尺寸
// 缩放比例
    const float fgScale = 1.0f / 6.0f;
    const float bgScale = 1.0f / 5.0f;
    const float overlapFactor = 0.48f;

// 计算缩放后的宽高
    fgDesc.displayWidth = static_cast<float>(fgDesc.imgWidth) * fgScale;
    fgDesc.displayHeight = static_cast<float>(fgDesc.imgHeight) * fgScale;
    bgDesc.displayWidth = static_cast<float>(bgDesc.imgWidth) * bgScale;
    bgDesc.displayHeight = static_cast<float>(bgDesc.imgHeight) * bgScale;

//    水平方向两图居中
    fgDesc.posX = ( static_cast<float>(width) / 2.0f )
                  - ( fgDesc.displayWidth / 2.0f );
    bgDesc.posX = ( static_cast<float>(width) / 2.0f )
                  - ( bgDesc.displayWidth / 2.0f );

//    垂直方向重叠一小部分
    float overlapAmount = fgDesc.displayHeight * overlapFactor;
    float combinedRenderHeight = fgDesc.displayHeight
                                 + bgDesc.displayHeight
                                 - overlapAmount;
    float screenCenterY = static_cast<float>(height) / 2.0f;
    fgDesc.posY = screenCenterY - ( combinedRenderHeight / 2.0f );
    bgDesc.posY =
            screenCenterY
            - ( combinedRenderHeight / 2.0f )
            + fgDesc.displayHeight
            + overlapAmount;
// end 设置尺寸
// 使用
    static auto glDrawer = glDisplayer::GlBitmapRenderer();
    glDrawer.drawComposite(bgDesc, fgDesc,width,height);

// 解锁像素
    AndroidBitmap_unlockPixels(env, fgBitmap);
    AndroidBitmap_unlockPixels(env, bgBitmap);

    // End of calling drawBitmap
    // 15. 交换缓冲区
    eglSwapBuffers(display, eglSurface);
    //资源清理
    ANativeWindow_release(window);
}

// 传入裸像素指针和尺寸，完成纹理上传和绘制全屏四边形
