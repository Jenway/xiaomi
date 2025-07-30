#!/bin/bash

export ANDROID_NDK="$(pwd)/android-ndk-r27d"

rm -rf build
mkdir build

cmake -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI="armeabi-v7a" \
    -DANDROID_NDK=$ANDROID_NDK \
    -DANDROID_PLATFORM=android-22 \
    -Bbuild ./cJSON

cmake --build build

