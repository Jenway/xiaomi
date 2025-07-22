#!/bin/bash

set -e

export ANDROID_NDK="/home/jenway/xiaomi/Day08/android-ndk-r27d"
export FFMPEG_BUILD_BASE_DIR="/home/jenway/xiaomi/final/build_ffmpeg_shared/out"

ANDROID_ABIS=("arm64-v8a" "x86_64")
ANDROID_PLATFORM=android-31
CMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake"

PROJECT_ROOT=$(pwd)

for ABI in "${ANDROID_ABIS[@]}"; do
    BUILD_DIR="$PROJECT_ROOT/build_android/$ABI"
    FFMPEG_ABI_DIR="$FFMPEG_BUILD_BASE_DIR/$ABI"

    echo ">> Building for ABI: $ABI"
    echo ">> FFmpeg libraries for this ABI expected at: $FFMPEG_ABI_DIR"

    if [ ! -d "$FFMPEG_ABI_DIR" ]; then
        echo "Error: FFmpeg build directory for $ABI not found at $FFMPEG_ABI_DIR"
        exit 1
    fi

    rm -rf "$BUILD_DIR"
    mkdir -p "$BUILD_DIR"

    cmake -S . -B "$BUILD_DIR" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE \
        -DANDROID_ABI=$ABI \
        -DANDROID_PLATFORM=$ANDROID_PLATFORM \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_ANDROID_LIB=ON \
        -DFFMPEG_ANDROID_INCLUDE_DIR="$FFMPEG_ABI_DIR/include" \
        -DFFMPEG_ANDROID_LIB_DIR="$FFMPEG_ABI_DIR/lib_static" \
        .

    cmake --build "$BUILD_DIR" -- -j$(nproc)

    echo ">> Output for $ABI is at: $BUILD_DIR"
    echo
done

# Optional for IDEs.
ln -sf build_android/arm64-v8a/compile_commands.json compile_commands.json

echo "Build process completed for specified ABIs."