//
// Created by Jenway on 2025/7/22.
//
#include <jni.h>
#include <android/native_window_jni.h>
#include <android/log.h>
#include <memory>
#include "NativePlayer.hpp" // 假设 NativePlayer.hpp 定义了具有所需方法的 C++ 播放器类

#define LOG_TAG "PlayerJNI"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// 帮助程序，用于将 jlong 转换为 NativePlayer 指针
static NativePlayer* getPlayer(jlong handle) {
    return reinterpret_cast<NativePlayer*>(handle);
}

static std::shared_ptr<NativePlayer>* getPlayerPtr(jlong handle) {
    return reinterpret_cast<std::shared_ptr<NativePlayer>*>(handle);
}

// 全局变量，用于缓存字段 ID 以提高性能
static jfieldID g_nativeContext_fieldID = nullptr;

// 在加载库时由 JVM 调用
extern "C" jint JNI_OnLoad(JavaVM* vm, void*) {
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }

    // 查找 Player 类
    jclass player_class = env->FindClass("com/example/androidplayer/Player");
    if (player_class == nullptr) {
        LOGE("找不到类 com/example/androidplayer/Player");
        return JNI_ERR;
    }

    // 获取并缓存 nativeContext 字段的 ID
    g_nativeContext_fieldID = env->GetFieldID(player_class, "nativeContext", "J");
    if (g_nativeContext_fieldID == nullptr) {
        LOGE("找不到字段 nativeContext");
        return JNI_ERR;
    }

    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativePlay(JNIEnv* env, jobject thiz, jstring file, jobject surface) {

    jlong handle = env->GetLongField(thiz, g_nativeContext_fieldID);
    auto player_ptr = getPlayerPtr(handle);

    if (player_ptr == nullptr) {
        // 创建一个 shared_ptr 并将其存储在堆上
        player_ptr = new std::shared_ptr<NativePlayer>(std::make_shared<NativePlayer>());
        LOGI("创建了新的 shared_ptr<NativePlayer> 实例: %p", player_ptr->get());
        env->SetLongField(thiz, g_nativeContext_fieldID, reinterpret_cast<jlong>(player_ptr));
    }

    const char* c_path = env->GetStringUTFChars(file, nullptr);
    if (!c_path) {
        LOGE("从 jstring 获取 C 字符串失败");
        return -1; // 表示错误
    }

    ANativeWindow* window = ANativeWindow_fromSurface(env, surface);
    if (window == nullptr) {
        LOGE("从 surface 获取 ANativeWindow 失败");
        env->ReleaseStringUTFChars(file, c_path);
        return -1;
    }

    // 假设 NativePlayer::play 存在
    (*player_ptr)->play(c_path, window); // 使用 -> 调用方法


    env->ReleaseStringUTFChars(file, c_path);
    // ANativeWindow 现在由原生播放器管理，此处无需释放
    return 0;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_androidplayer_Player_nativePause(JNIEnv* env, jobject thiz, jboolean p) {
    jlong handle = env->GetLongField(thiz, g_nativeContext_fieldID);

    auto player_sptr_ptr = getPlayerPtr(handle); // Cast to std::shared_ptr<NativePlayer>*
    if (player_sptr_ptr) {
        NativePlayer* player = player_sptr_ptr->get(); // Get the raw pointer from the shared_ptr
        if (player) {
            player->pause(p);
        }
    } else {
        LOGE("在空播放器实例上调用了 nativePause");
    }
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativeStop(JNIEnv* env, jobject thiz) {
    jlong handle = env->GetLongField(thiz, g_nativeContext_fieldID);
    auto player_ptr = getPlayerPtr(handle);

    if (player_ptr) {
        LOGI("销毁 shared_ptr<NativePlayer> 实例: %p", player_ptr->get());
        (*player_ptr)->stop(); // 先调用 stop 逻辑
        delete player_ptr; // 删除 shared_ptr 对象本身

        env->SetLongField(thiz, g_nativeContext_fieldID, 0);
    } else {
        LOGE("在空播放器实例上调用了 nativeStop");
    }
    return 0; // 按照 Java 声明返回 int
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativeSeek(JNIEnv* env, jobject thiz, jdouble position) {
    jlong handle = env->GetLongField(thiz, g_nativeContext_fieldID);
    auto player_sptr_ptr = getPlayerPtr(handle); // Cast to std::shared_ptr<NativePlayer>*
    if (player_sptr_ptr) {
        NativePlayer* player = player_sptr_ptr->get(); // Get the raw pointer from the shared_ptr
        if (player) {
            player->seek(position); // 假设 NativePlayer::seek 存在
            return 0; // 假设成功
        }
    } else {
        LOGE("在空播放器实例上调用了 nativeSeek");
        return -1; // 表示错误
    }
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_androidplayer_Player_nativeSetSpeed(JNIEnv* env, jobject thiz, jfloat speed) {
    jlong handle = env->GetLongField(thiz, g_nativeContext_fieldID);
    NativePlayer* player = getPlayer(handle);
    if (player) {
        // player->set_speed(speed); // 假设 NativePlayer::set_speed 存在
        LOGI("nativeSetSpeed 调用，速度为: %f", speed);
        return 0;
    } else {
        LOGE("在空播放器实例上调用了 nativeSetSpeed");
        return -1;
    }
}

extern "C" JNIEXPORT jdouble JNICALL
Java_com_example_androidplayer_Player_nativeGetPosition(JNIEnv* env, jobject thiz) {
    jlong handle = env->GetLongField(thiz, g_nativeContext_fieldID);
    NativePlayer* player = getPlayer(handle);
    if (player) {
        // return player->get_position(); // 假设 NativePlayer::get_position 存在
        return 0.0; // 占位符
    } else {
        LOGE("在空播放器实例上调用了 nativeGetPosition");
        return 0.0;
    }
}

extern "C" JNIEXPORT jdouble JNICALL
Java_com_example_androidplayer_Player_nativeGetDuration(JNIEnv* env, jobject thiz) {
    jlong handle = env->GetLongField(thiz, g_nativeContext_fieldID);
    NativePlayer* player = getPlayer(handle);
    if (player) {
        // return player->get_duration(); // 假设 NativePlayer::get_duration 存在
        return 0.0; // 占位符
    } else {
        LOGE("在空播放器实例上调用了 nativeGetDuration");
        return 0.0;
    }
}