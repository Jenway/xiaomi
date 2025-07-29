#include "jni_helpers.hpp"

namespace jni {

Mp4Parser* getMp4Parser(JNIEnv* env, jobject obj)
{
    jfieldID fieldId = env->GetFieldID(env->GetObjectClass(obj), "nativeHandle", "J");
    if (fieldId == nullptr) {
        LOGE("Failed to get nativeHandle field ID");
        return nullptr;
    }
    long handle = env->GetLongField(obj, fieldId);
    return reinterpret_cast<Mp4Parser*>(handle);
}

void setMp4Parser(JNIEnv* env, jobject obj, Mp4Parser* parser)
{
    jfieldID fieldId = env->GetFieldID(env->GetObjectClass(obj), "nativeHandle", "J");
    if (fieldId == nullptr) {
        LOGE("Failed to set nativeHandle field ID");
        return;
    }
    env->SetLongField(obj, fieldId, reinterpret_cast<jlong>(parser));
}

void handleJniException(JNIEnv* env, const char* errorMessage)
{
    if (env->ExceptionCheck()) {
        LOGE("%s - JNI Exception occurred:", errorMessage);
        env->ExceptionDescribe();
        env->ExceptionClear();
    } else {
        LOGE("%s", errorMessage);
    }
}

} // namespace jni