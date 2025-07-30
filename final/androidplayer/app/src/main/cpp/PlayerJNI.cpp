//
// Created by Jenway on 2025/7/22.
//
#include <jni.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <memory>
#include "NativePlayer.hpp"

#define LOG_TAG "PlayerJNI"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

static JavaVM* g_vm = nullptr;
static jfieldID g_nativeContext_fieldID = nullptr;

static std::shared_ptr<NativePlayer>* getPlayerSharePtr(jlong handle) {
    if (handle == 0) {
        return nullptr;
    }
    return reinterpret_cast<std::shared_ptr<NativePlayer>*>(handle);
}

extern "C" jint JNI_OnLoad(JavaVM* vm, void*) {
    g_vm = vm; // 缓存 JavaVM
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }

    jclass player_class = env->FindClass("com/example/androidplayer/Player");
    if (player_class == nullptr) {
        LOGE("找不到类 com/example/androidplayer/Player");
        return JNI_ERR;
    }

    g_nativeContext_fieldID = env->GetFieldID(player_class, "nativeContext", "J");
    if (g_nativeContext_fieldID == nullptr) {
        LOGE("找不到字段 nativeContext");
        return JNI_ERR;
    }

    return JNI_VERSION_1_6;
}

// --- Native 方法实现 ---

extern "C" JNIEXPORT void JNICALL
Java_com_example_androidplayer_Player_nativeInit(JNIEnv* env, jobject thiz) {
    // 1. 创建 C++ 播放器实例，由 shared_ptr 管理
    auto player = std::make_shared<NativePlayer>();

    // 2. 将 JNI 上下文传递给 C++ 层，用于实现回调
    //    NewGlobalRef 确保 Java 对象在 native 代码中不会被 GC 回收
    player->setJniEnv(g_vm, env->NewGlobalRef(thiz));

    // 3. 在堆上创建一个 shared_ptr 的副本，并将这个副本的指针存入 jlong
    //    这是将 C++ 对象生命周期与 Java 对象绑定的关键
    auto* sptr_ptr_on_heap = new std::shared_ptr<NativePlayer>(player);

    env->SetLongField(thiz, g_nativeContext_fieldID, reinterpret_cast<jlong>(sptr_ptr_on_heap));
    LOGI("NativePlayer a new instance created and initialized. Handle: %p", sptr_ptr_on_heap);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_androidplayer_Player_nativeRelease(JNIEnv* env, jobject thiz) {
    jlong handle = env->GetLongField(thiz, g_nativeContext_fieldID);
    auto sptr_ptr = getPlayerSharePtr(handle);

    if (sptr_ptr) {
        LOGI("NativePlayer instance releasing. Handle: %p", sptr_ptr);
        // delete 堆上的 shared_ptr 对象。
        // 这会触发 shared_ptr 的析构函数，如果引用计数为零，
        // 则会自动调用 NativePlayer 的析构函数，从而执行所有清理工作。
        delete sptr_ptr;
        env->SetLongField(thiz, g_nativeContext_fieldID, 0); // 清空句柄，防止野指针
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_androidplayer_Player_nativePlay(JNIEnv* env, jobject thiz, jstring file, jobject surface) {
    auto sptr_ptr = getPlayerSharePtr(env->GetLongField(thiz, g_nativeContext_fieldID));
    if (!sptr_ptr) {
        LOGE("nativePlay called on a released player.");
        return;
    }

    const char* c_path = env->GetStringUTFChars(file, nullptr);
    if (!c_path) {
        LOGE("Failed to get C-string from jstring.");
        return;
    }

    // ANativeWindow 的生命周期现在应该由 NativePlayer 内部管理（acquire/release）
    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (window == nullptr) {
        LOGE("Failed to get ANativeWindow from surface.");
        env->ReleaseStringUTFChars(file, c_path);
        return;
    }

    (*sptr_ptr)->play(c_path, window);

    env->ReleaseStringUTFChars(file, c_path);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_androidplayer_Player_nativePause(JNIEnv* env, jobject thiz, jboolean p) {
    auto sptr_ptr = getPlayerSharePtr(env->GetLongField(thiz, g_nativeContext_fieldID));
    if (sptr_ptr) {
        (*sptr_ptr)->pause(p);
    }
}
extern "C" JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativeGetState(JNIEnv *env, jobject thiz) {
    auto sptr_ptr = getPlayerSharePtr(env->GetLongField(thiz, g_nativeContext_fieldID));
    if (sptr_ptr) {
        return static_cast<jint>((*sptr_ptr)->getState());
    }
    return static_cast<jint>(player_utils::PlayerState::None);
}

extern "C" JNIEXPORT jdouble JNICALL
Java_com_example_androidplayer_Player_nativeGetPosition(JNIEnv *env, jobject thiz) {
    auto sptr_ptr = getPlayerSharePtr(env->GetLongField(thiz, g_nativeContext_fieldID));
    if (sptr_ptr) {
        return (*sptr_ptr)->getPosition();
    }
    return 0.0;
}
extern "C" JNIEXPORT void JNICALL
Java_com_example_androidplayer_Player_nativeStop(JNIEnv* env, jobject thiz) {
    auto sptr_ptr = getPlayerSharePtr(env->GetLongField(thiz, g_nativeContext_fieldID));
    if (sptr_ptr) {
        (*sptr_ptr)->stop();
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_androidplayer_Player_nativeSeek(JNIEnv* env, jobject thiz, jdouble position) {
    auto sptr_ptr = getPlayerSharePtr(env->GetLongField(thiz, g_nativeContext_fieldID));
    if (sptr_ptr) {
        (*sptr_ptr)->seek(position);
    }
}

extern "C" JNIEXPORT jdouble JNICALL
Java_com_example_androidplayer_Player_nativeGetDuration(JNIEnv* env, jobject thiz) {
    auto sptr_ptr = getPlayerSharePtr(env->GetLongField(thiz, g_nativeContext_fieldID));
    if (sptr_ptr) {
        return (*sptr_ptr)->getDuration();
    }
    return 0.0;
}