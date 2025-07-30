#include <jni.h>

extern "C" {
#include <libavutil/version.h>
#include <libavutil/avutil.h>
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_example_app_MainActivity_getFFmpegVersion(JNIEnv *env, jobject) {
    return env->NewStringUTF(av_version_info());
}
