#ifndef JNI_HELPERS_HPP
#define JNI_HELPERS_HPP

#include "mp4_parser.hpp"
#include <android/log.h>
#include <jni.h>

// Define a tag for logging
#define TAG "Mp4ParserJNI"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)

namespace jni {

Mp4Parser* getMp4Parser(JNIEnv* env, jobject obj);

void setMp4Parser(JNIEnv* env, jobject obj, Mp4Parser* parser);

void handleJniException(JNIEnv* env, const char* errorMessage);

} // namespace jni

#endif // JNI_HELPERS_HPP