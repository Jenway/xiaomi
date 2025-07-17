#!/bin/bash

set -e

export ANDROID_NDK="/home/jenway/xiaomi/Day08/android-ndk-r27d"

ANDROID_ABIS=("armeabi-v7a" "arm64-v8a" "x86" "x86_64")
ANDROID_PLATFORM=android-31
CMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake"

for ABI in "${ANDROID_ABIS[@]}"; do
    BUILD_DIR="build/$ABI"
    echo ">> Building for ABI: $ABI"

    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"

    cmake -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE \
        -DANDROID_ABI=$ABI \
        -DANDROID_PLATFORM=$ANDROID_PLATFORM \
        -DCMAKE_BUILD_TYPE=Release \
        .

    cmake --build "$BUILD_DIR" -- -j$(nproc)

    echo ">> Output .so is at: $BUILD_DIR/libmath_utils_jni.so"
    echo
done
