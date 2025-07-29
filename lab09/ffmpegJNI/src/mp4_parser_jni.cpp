#include "mp4_parser_jni.hpp"
#include "jni_helpers.hpp"
#include "mp4_parser.hpp"
#include <vector>

using namespace jni;

// Implements Mp4ParserJNI.open(String filePath)
extern "C" JNIEXPORT jboolean JNICALL Java_com_example_ffmpeg_Mp4ParserJNI_open(
    JNIEnv* env, jobject thiz, jstring filePath)
{
    auto* parser = new Mp4Parser();
    if (!parser) {
        LOGE("Failed to allocate Mp4Parser object.");
        return JNI_FALSE;
    }

    const char* nativeFilePath = env->GetStringUTFChars(filePath, nullptr);
    if (!nativeFilePath) {
        handleJniException(env, "Failed to get UTF-8 chars from file path.");
        delete parser;
        return JNI_FALSE;
    }
    std::string cppFilePath(nativeFilePath);
    env->ReleaseStringUTFChars(filePath, nativeFilePath);

    bool success = parser->open(cppFilePath);

    if (success) {
        setMp4Parser(env, thiz, parser);
        LOGI("Mp4Parser opened successfully for %s", cppFilePath.c_str());
        return JNI_TRUE;
    } else {
        LOGE("Failed to open Mp4Parser for %s", cppFilePath.c_str());
        delete parser;
        setMp4Parser(env, thiz, nullptr);
        return JNI_FALSE;
    }
}

// Implements Mp4ParserJNI.readNextVideoPacket()
extern "C" JNIEXPORT jobject JNICALL Java_com_example_ffmpeg_Mp4ParserJNI_readNextVideoPacket(
    JNIEnv* env, jobject thiz)
{
    Mp4Parser* parser = getMp4Parser(env, thiz);
    if (!parser) {
        handleJniException(env, "Mp4Parser object is null in readNextVideoPacket. Did you call open()?");
        return nullptr;
    }

    PacketInfo packetInfo;
    bool success = parser->readNextVideoPacket(packetInfo);

    if (success) {
        jclass packetInfoClass = env->FindClass("com/example/ffmpeg/Mp4ParserJNI$PacketInfo");
        if (packetInfoClass == nullptr) {
            handleJniException(env, "Failed to find PacketInfo class.");
            return nullptr;
        }

        jmethodID constructor = env->GetMethodID(packetInfoClass, "<init>", "()V");
        if (constructor == nullptr) {
            handleJniException(env, "Failed to find PacketInfo constructor.");
            env->DeleteLocalRef(packetInfoClass);
            return nullptr;
        }

        jobject javaPacketInfo = env->NewObject(packetInfoClass, constructor);
        if (javaPacketInfo == nullptr) {
            handleJniException(env, "Failed to create new PacketInfo object.");
            env->DeleteLocalRef(packetInfoClass);
            return nullptr;
        }

        jfieldID ptsField = env->GetFieldID(packetInfoClass, "pts", "J");
        jfieldID dtsField = env->GetFieldID(packetInfoClass, "dts", "J");
        jfieldID streamIndexField = env->GetFieldID(packetInfoClass, "streamIndex", "I");

        if (ptsField == nullptr || dtsField == nullptr || streamIndexField == nullptr) {
            handleJniException(env, "Failed to find PacketInfo fields.");
            env->DeleteLocalRef(packetInfoClass);
            env->DeleteLocalRef(javaPacketInfo);
            return nullptr;
        }

        env->SetLongField(javaPacketInfo, ptsField, packetInfo.pts);
        env->SetLongField(javaPacketInfo, dtsField, packetInfo.dts);
        env->SetIntField(javaPacketInfo, streamIndexField, packetInfo.streamIndex);

        env->DeleteLocalRef(packetInfoClass);
        return javaPacketInfo;
    } else {
        LOGI("No more video packets or error reading.");
        return nullptr;
    }
}

// Implements Mp4ParserJNI.readAllVideoPackets()
extern "C" JNIEXPORT jobject JNICALL Java_com_example_ffmpeg_Mp4ParserJNI_readAllVideoPackets(
    JNIEnv* env, jobject thiz)
{
    Mp4Parser* parser = getMp4Parser(env, thiz);
    if (!parser) {
        handleJniException(env, "Mp4Parser object is null in readAllVideoPackets. Did you call open()?");
        return nullptr;
    }

    std::vector<PacketInfo> cppPackets = parser->readAllVideoPackets();

    jclass arrayListClass = env->FindClass("java/util/ArrayList");
    if (arrayListClass == nullptr) {
        handleJniException(env, "Failed to find ArrayList class.");
        return nullptr;
    }
    jmethodID arrayListConstructor = env->GetMethodID(arrayListClass, "<init>", "(I)V");
    jmethodID arrayListAdd = env->GetMethodID(arrayListClass, "add", "(Ljava/lang/Object;)Z");
    if (arrayListConstructor == nullptr || arrayListAdd == nullptr) {
        handleJniException(env, "Failed to find ArrayList methods.");
        env->DeleteLocalRef(arrayListClass);
        return nullptr;
    }

    jobject javaList = env->NewObject(arrayListClass, arrayListConstructor, (jint)cppPackets.size());
    if (javaList == nullptr) {
        handleJniException(env, "Failed to create new ArrayList object.");
        env->DeleteLocalRef(arrayListClass);
        return nullptr;
    }

    jclass packetInfoClass = env->FindClass("com/example/ffmpeg/Mp4ParserJNI$PacketInfo");
    if (packetInfoClass == nullptr) {
        handleJniException(env, "Failed to find PacketInfo class for list.");
        env->DeleteLocalRef(arrayListClass);
        env->DeleteLocalRef(javaList);
        return nullptr;
    }
    jmethodID packetInfoConstructor = env->GetMethodID(packetInfoClass, "<init>", "()V");
    jfieldID ptsField = env->GetFieldID(packetInfoClass, "pts", "J");
    jfieldID dtsField = env->GetFieldID(packetInfoClass, "dts", "J");
    jfieldID streamIndexField = env->GetFieldID(packetInfoClass, "streamIndex", "I");

    if (packetInfoConstructor == nullptr || ptsField == nullptr || dtsField == nullptr || streamIndexField == nullptr) {
        handleJniException(env, "Failed to find PacketInfo constructor/fields for list.");
        env->DeleteLocalRef(arrayListClass);
        env->DeleteLocalRef(javaList);
        env->DeleteLocalRef(packetInfoClass);
        return nullptr;
    }

    for (const auto& cppPacket : cppPackets) {
        jobject javaPacketInfo = env->NewObject(packetInfoClass, packetInfoConstructor);
        if (javaPacketInfo == nullptr) {
            handleJniException(env, "Failed to create new PacketInfo object in list loop.");
            continue;
        }
        env->SetLongField(javaPacketInfo, ptsField, cppPacket.pts);
        env->SetLongField(javaPacketInfo, dtsField, cppPacket.dts);
        env->SetIntField(javaPacketInfo, streamIndexField, cppPacket.streamIndex);
        env->CallBooleanMethod(javaList, arrayListAdd, javaPacketInfo);
        env->DeleteLocalRef(javaPacketInfo);
    }

    env->DeleteLocalRef(arrayListClass);
    env->DeleteLocalRef(packetInfoClass);
    return javaList;
}

// Implements Mp4ParserJNI.decodeNextVideoFrame()
extern "C" JNIEXPORT jobject JNICALL Java_com_example_ffmpeg_Mp4ParserJNI_decodeNextVideoFrame(
    JNIEnv* env, jobject thiz)
{
    Mp4Parser* parser = getMp4Parser(env, thiz);
    if (!parser) {
        handleJniException(env, "Mp4Parser object is null in decodeNextVideoFrame. Did you call open()?");
        return nullptr;
    }

    FrameInfo frameInfo;
    bool success = parser->decodeNextVideoFrame(frameInfo);

    if (success) {
        jclass frameInfoClass = env->FindClass("com/example/ffmpeg/Mp4ParserJNI$FrameInfo");
        if (frameInfoClass == nullptr) {
            handleJniException(env, "Failed to find FrameInfo class.");
            return nullptr;
        }

        jmethodID constructor = env->GetMethodID(frameInfoClass, "<init>", "()V");
        if (constructor == nullptr) {
            handleJniException(env, "Failed to find FrameInfo constructor.");
            env->DeleteLocalRef(frameInfoClass);
            return nullptr;
        }

        jobject javaFrameInfo = env->NewObject(frameInfoClass, constructor);
        if (javaFrameInfo == nullptr) {
            handleJniException(env, "Failed to create new FrameInfo object.");
            env->DeleteLocalRef(frameInfoClass);
            return nullptr;
        }

        jfieldID widthField = env->GetFieldID(frameInfoClass, "width", "I");
        jfieldID heightField = env->GetFieldID(frameInfoClass, "height", "I");
        jfieldID formatField = env->GetFieldID(frameInfoClass, "format", "I");
        jfieldID ptsField = env->GetFieldID(frameInfoClass, "pts", "J");

        if (widthField == nullptr || heightField == nullptr || formatField == nullptr || ptsField == nullptr) {
            handleJniException(env, "Failed to find FrameInfo fields.");
            env->DeleteLocalRef(frameInfoClass);
            env->DeleteLocalRef(javaFrameInfo);
            return nullptr;
        }

        env->SetIntField(javaFrameInfo, widthField, frameInfo.width);
        env->SetIntField(javaFrameInfo, heightField, frameInfo.height);
        env->SetIntField(javaFrameInfo, formatField, (jint)frameInfo.format);
        env->SetLongField(javaFrameInfo, ptsField, frameInfo.pts);

        env->DeleteLocalRef(frameInfoClass);
        return javaFrameInfo;
    } else {
        LOGI("No more video frames or error decoding.");
        return nullptr;
    }
}

// Implements Mp4ParserJNI.dumpAllFramesToYUV(String yuvOutputPath)
extern "C" JNIEXPORT jboolean JNICALL Java_com_example_ffmpeg_Mp4ParserJNI_dumpAllFramesToYUV(
    JNIEnv* env, jobject thiz, jstring yuvOutputPath)
{
    Mp4Parser* parser = getMp4Parser(env, thiz);
    if (!parser) {
        handleJniException(env, "Mp4Parser object is null in dumpAllFramesToYUV. Did you call open()?");
        return JNI_FALSE;
    }

    const char* nativeYuvOutputPath = env->GetStringUTFChars(yuvOutputPath, nullptr);
    if (!nativeYuvOutputPath) {
        handleJniException(env, "Failed to get UTF-8 chars from YUV output path.");
        return JNI_FALSE;
    }
    std::string cppYuvOutputPath(nativeYuvOutputPath);
    env->ReleaseStringUTFChars(yuvOutputPath, nativeYuvOutputPath);

    bool success = parser->dumpAllFramesToYUV(cppYuvOutputPath);

    if (success) {
        LOGI("Successfully dumped all frames to YUV: %s", cppYuvOutputPath.c_str());
        return JNI_TRUE;
    } else {
        LOGE("Failed to dump all frames to YUV: %s", cppYuvOutputPath.c_str());
        return JNI_FALSE;
    }
}

// Mp4ParserJNI.close()
extern "C" JNIEXPORT void JNICALL Java_com_example_ffmpeg_Mp4ParserJNI_close(
    JNIEnv* env, jobject thiz)
{
    Mp4Parser* parser = getMp4Parser(env, thiz);
    if (parser) {
        parser->close();
        delete parser;
        setMp4Parser(env, thiz, nullptr);
        LOGI("Mp4Parser closed and resources freed.");
    } else {
        LOGW("Attempted to close a null Mp4Parser object.");
    }
}