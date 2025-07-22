#!/bin/bash

set -e

export ANDROID_NDK="/home/jenway/xiaomi/Day08/android-ndk-r27d"
# Base directory for pre-built FFmpeg libraries
export FFMPEG_BUILD_BASE_DIR="/home/jenway/xiaomi/final/build_ffmpeg_shared/out"


# Only target arm64-v8a and x86_64 as requested
ANDROID_ABIS=("arm64-v8a" "x86_64")
ANDROID_PLATFORM=android-31
CMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake"

for ABI in "${ANDROID_ABIS[@]}"; do
    BUILD_DIR="build_android/$ABI" # Changed build directory naming for clarity
    FFMPEG_ABI_DIR="$FFMPEG_BUILD_BASE_DIR/$ABI" # Path to FFmpeg for current ABI

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

    # Note: The output .so name will depend on your CMakeLists.txt's add_library name.
    # Assuming it's `mp4parser_jni` based on the JNI wrapper example.
    echo ">> Output .so for $ABI is at: $BUILD_DIR/libmp4parser_jni.so"
    echo
done

# Optional: Create a symlink for compile_commands.json from one of the builds for tooling
# Using arm64-v8a as the default for convenience if you need it for IDEs.
ln -sf build_android/arm64-v8a/compile_commands.json compile_commands.json

echo "Build process completed for specified ABIs."