#include "JniCallbackHandler.hpp"
#include <android/log.h>

#define LOG_TAG "JniCallbackHandler"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

/**
 * @brief 一个辅助类，使用 RAII 技术来安全地管理 JNIEnv 的获取和线程分离。
 */
struct JniEnvGuard {
    JavaVM* jvm = nullptr;
    JNIEnv* env = nullptr;
    bool attached = false;

    explicit JniEnvGuard(JavaVM* vm)
        : jvm(vm)
    {
        if (jvm == nullptr)
            return;
        if (jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
            if (jvm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
                attached = true;
            } else {
                LOGE("Failed to attach current thread to JVM");
                env = nullptr;
            }
        }
    }

    ~JniEnvGuard()
    {
        if (attached && jvm != nullptr) {
            jvm->DetachCurrentThread();
        }
    }

    // 提供一个简便的方式来获取 env 指针
    JNIEnv* get() const { return env; }
};

JniCallbackHandler::JniCallbackHandler(JavaVM* vm, jobject global_player_object)
    : jvm_(vm)
    , jni_player_object_(global_player_object)
{
    LOGI("JniCallbackHandler constructor: Initializing...");
    if (jvm_ == nullptr || jni_player_object_ == nullptr) {
        LOGE("JNI VM or player object is null.");
        return;
    }

    // 在构造时就获取并缓存 Method ID
    JniEnvGuard guard(jvm_);
    JNIEnv* env = guard.get();
    if (!env) {
        LOGE("Failed to get JNIEnv in constructor.");
        return;
    }

    jclass player_class = env->GetObjectClass(jni_player_object_);
    if (player_class == nullptr) {
        LOGE("Failed to find player class.");
        return;
    }

    on_state_changed_mid_ = env->GetMethodID(player_class, "onNativeStateChanged", "(I)V");
    if (on_state_changed_mid_ == nullptr) {
        LOGE("Failed to find method 'onNativeStateChanged(I)V'.");
    } else {
        LOGI("Successfully cached 'onNativeStateChanged' method ID.");
    }

    // JNI 规范要求删除局部引用
    env->DeleteLocalRef(player_class);
}

JniCallbackHandler::~JniCallbackHandler()
{
    LOGI("JniCallbackHandler destructor: Releasing JNI global reference.");

    JniEnvGuard guard(jvm_);
    JNIEnv* env = guard.get();
    if (env && jni_player_object_) {
        env->DeleteGlobalRef(jni_player_object_);
        jni_player_object_ = nullptr;
    }
}

void JniCallbackHandler::notifyStateChanged(player_utils::PlayerState newState)
{
    // 如果构造时初始化失败，直接返回
    if (!jvm_ || !jni_player_object_ || !on_state_changed_mid_) {
        return;
    }

    JniEnvGuard guard(jvm_);
    JNIEnv* env = guard.get();
    if (!env) {
        return;
    }

    // 直接调用，不再进行状态检查。这是调用者的责任。
    env->CallVoidMethod(jni_player_object_, on_state_changed_mid_, static_cast<jint>(newState));
}