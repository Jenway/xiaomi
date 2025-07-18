#include "jni_helpers.hpp" // For LOGI and LOGW macros
#include <jni.h>

// JNI_OnLoad is called when the JNI library is loaded.
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    JNIEnv* env;
    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    LOGI("JNI_OnLoad called for Mp4ParserJNI");
    return JNI_VERSION_1_6;
}

// JNI_OnUnload is called when the JNI library is unloaded.
JNIEXPORT void JNI_OnUnload(JavaVM* vm, void* reserved)
{
    LOGI("JNI_OnUnload called for Mp4ParserJNI");
}