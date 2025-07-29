#ifndef MP4_PARSER_JNI_HPP
#define MP4_PARSER_JNI_HPP

#include <jni.h>

#ifdef __cplusplus
extern "C" {
#endif
// Package: com.example.ffmpeg.Mp4ParserJNI
JNIEXPORT jboolean JNICALL Java_com_example_ffmpeg_Mp4ParserJNI_open(JNIEnv* env, jobject thiz, jstring filePath);
JNIEXPORT jobject JNICALL Java_com_example_ffmpeg_Mp4ParserJNI_readNextVideoPacket(JNIEnv* env, jobject thiz);
JNIEXPORT jobject JNICALL Java_com_example_ffmpeg_Mp4ParserJNI_readAllVideoPackets(JNIEnv* env, jobject thiz);
JNIEXPORT jobject JNICALL Java_com_example_ffmpeg_Mp4ParserJNI_decodeNextVideoFrame(JNIEnv* env, jobject thiz);
JNIEXPORT jboolean JNICALL Java_com_example_ffmpeg_Mp4ParserJNI_dumpAllFramesToYUV(JNIEnv* env, jobject thiz, jstring yuvOutputPath);
JNIEXPORT void JNICALL Java_com_example_ffmpeg_Mp4ParserJNI_close(JNIEnv* env, jobject thiz);

#ifdef __cplusplus
}
#endif

#endif // MP4_PARSER_JNI_HPP