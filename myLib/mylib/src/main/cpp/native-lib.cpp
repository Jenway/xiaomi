#include <jni.h>
#include <string>

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_mylib_stringToFromJNI(JNIEnv *env, jobject /* this */) {
    std::string hello = "Hello from JNI calling C++";
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_mylib_add(JNIEnv *env, jobject /* this */, jint a, jint b) {
    return a + b;
}