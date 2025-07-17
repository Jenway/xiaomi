// math_utils_jni.cpp
#include "math_utils.hpp"
#include <jni.h>

#define DEFINE_MATH_JNI(Type, JniType, JavaName)                                    \
    extern "C" {                                                                    \
    JNIEXPORT JniType JNICALL Java_com_example_mathutils_MathUtils_##JavaName##Add( \
        JNIEnv*, jobject, JniType a, JniType b)                                     \
    {                                                                               \
        return math_utils::add<Type>(a, b);                                         \
    }                                                                               \
    JNIEXPORT JniType JNICALL Java_com_example_mathutils_MathUtils_##JavaName##Sub( \
        JNIEnv*, jobject, JniType a, JniType b)                                     \
    {                                                                               \
        return math_utils::sub<Type>(a, b);                                         \
    }                                                                               \
    JNIEXPORT JniType JNICALL Java_com_example_mathutils_MathUtils_##JavaName##Mul( \
        JNIEnv*, jobject, JniType a, JniType b)                                     \
    {                                                                               \
        return math_utils::mul<Type>(a, b);                                         \
    }                                                                               \
    JNIEXPORT JniType JNICALL Java_com_example_mathutils_MathUtils_##JavaName##Div( \
        JNIEnv*, jobject, JniType a, JniType b)                                     \
    {                                                                               \
        return math_utils::div<Type>(a, b);                                         \
    }                                                                               \
    }

DEFINE_MATH_JNI(int, jint, Int)
DEFINE_MATH_JNI(float, jfloat, Float)
DEFINE_MATH_JNI(double, jdouble, Double)
