package com.example;

public class mylib {

    static {
        System.loadLibrary("mylib");
    }

    public static native String stringToFromJNI();
    public static native int add(int a, int b);
}
