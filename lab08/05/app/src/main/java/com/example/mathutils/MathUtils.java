package com.example.mathutils;

public class MathUtils {
    static {
        System.loadLibrary("math_utils_jni");
    }

    public static native int IntAdd(int a, int b);

    public static native int IntSub(int a, int b);

    public static native int IntMul(int a, int b);

    public static native int IntDiv(int a, int b);

    public static native float FloatAdd(float a, float b);

    public static native float FloatSub(float a, float b);

    public static native float FloatMul(float a, float b);

    public static native float FloatDiv(float a, float b);

    public static native double DoubleAdd(double a, double b);

    public static native double DoubleSub(double a, double b);

    public static native double DoubleMul(double a, double b);

    public static native double DoubleDiv(double a, double b);
}
