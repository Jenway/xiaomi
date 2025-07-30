#!/bin/bash

set -e

export ANDROID_NDK="/home/jenway/xiaomi/Day08/android-ndk-r27d"

# Define the root directory where FFmpeg was installed/built
# Assuming FFmpeg output is in ../build/out relative to the current directory
FFMPEG_ROOT_DIR="/home/jenway/xiaomi/Day09/build/out"

ANDROID_ABIS=("arm64-v8a" "x86_64")
ANDROID_PLATFORM=android-31
CMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake"

for ABI in "${ANDROID_ABIS[@]}"; do
    BUILD_DIR="build/$ABI"
    echo ">> Building for ABI: $ABI"

    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"

    # Set FFmpeg include and lib directories for the current ABI
    # These variables will be passed to CMake
    export FFMPEG_INCLUDE_DIR="$FFMPEG_ROOT_DIR/$ABI/include"
    export FFMPEG_LIB_DIR="$FFMPEG_ROOT_DIR/$ABI/lib"

    cmake -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE \
        -DANDROID_ABI=$ABI \
        -DANDROID_PLATFORM=$ANDROID_PLATFORM \
        -DCMAKE_BUILD_TYPE=Release \
        -DFFMPEG_INCLUDE_DIR="$FFMPEG_INCLUDE_DIR" \
        -DFFMPEG_LIB_DIR="$FFMPEG_LIB_DIR" \
        .

    cmake --build "$BUILD_DIR" -- -j$(nproc)

    # Note: The output .so name in your original script was libmath_utils_jni.so,
    # but your project is named ffmpeg_utils_jni. I'm correcting it here.
    echo ">> Output .so is at: $BUILD_DIR/libffmpeg_utils_jni.so"
    echo
done

