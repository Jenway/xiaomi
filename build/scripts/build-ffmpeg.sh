#!/bin/bash
set -e

ANDROID_NDK=${ANDROID_NDK:-/opt/android/android-ndk-r27d}
FFMPEG_SRC=${FFMPEG_SRC:-/src/ffmpeg}
OUT_DIR=${OUT_DIR:-/build}
API=${API:-31}

COMMON_CONFIG=(
    --enable-cross-compile
    --target-os=android
    --disable-programs
    --disable-doc
    --disable-static
    --enable-shared
    --disable-everything
    --disable-symver

    # Zlib is required by mov/mp4 demuxer for compressed headers
    --enable-zlib  # <-- 添加这一行

    --disable-decoders
    --enable-decoder=h264

    --disable-encoders
    # --enable-encoder=libx264 # 如果您不需要编码功能，可以注释掉以减小体积

    --disable-parsers
    --enable-parser=h264

    --disable-libaom
    --disable-filters

    --disable-demuxers
    --enable-demuxer=mov

    --disable-muxers
    --enable-muxer=mp4

    # The h264_mp4toannexb BSF is essential for decoding H264 from MP4
    --enable-bsf=h264_mp4toannexb

    --disable-protocols
    --enable-protocol=file
    --enable-protocol=http
    --enable-protocol=https

    --disable-hwaccels
    --disable-devices
    --disable-postproc
    --disable-gpl
    --disable-version3
)


# This function is no longer strictly necessary if we define tools explicitly,
# but it doesn't hurt to keep it.
create_tool_links() {
    local TOOLCHAIN=$1
    local TARGET=$2
    for TOOL in strip nm ranlib ar; do
        local TARGET_TOOL=$TOOLCHAIN/bin/${TARGET}-$TOOL
        local LLVM_TOOL=$TOOLCHAIN/bin/llvm-$TOOL
        if [ ! -f "$TARGET_TOOL" ] && [ -f "$LLVM_TOOL" ]; then
            ln -sf "$LLVM_TOOL" "$TARGET_TOOL"
            echo ">> Linked $TARGET_TOOL -> $LLVM_TOOL"
        fi
    done
}

build_ffmpeg_for_abi() {
    local ABI=$1
    echo "========== Building for ABI: $ABI =========="

    case $ABI in
        arm64-v8a)
            ARCH=arm64
            TARGET=aarch64-linux-android
            CPU=armv8-a
            ;;
        x86_64)
            ARCH=x86_64
            TARGET=x86_64-linux-android
            CPU=x86-64
            ;;
        *)
            echo "Unsupported ABI: $ABI"
            return 1
            ;;
    esac

    TOOLCHAIN=$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64
    CC=$TOOLCHAIN/bin/${TARGET}${API}-clang
    # No need for CROSS_PREFIX if we are explicit

    # create_tool_links can still be run if you want the symlinks
    # create_tool_links "$TOOLCHAIN" "$TARGET"

    SYSROOT="$TOOLCHAIN/sysroot"
    export CFLAGS="--sysroot=$SYSROOT"
    export LDFLAGS="--sysroot=$SYSROOT -L$SYSROOT/usr/lib/$TARGET/$API"

    BUILD_DIR="$OUT_DIR/$ABI"
    INSTALL_DIR="$OUT_DIR/android/$ABI"
    mkdir -p "$BUILD_DIR" "$INSTALL_DIR"
    cd "$BUILD_DIR"

    # --- CHANGE 2: Call configure with the array and explicit tool paths ---
    echo ">> Configuring..."
    # Wrap the configure command to capture its failure
    if ! "$FFMPEG_SRC/configure" \
        --prefix="$INSTALL_DIR" \
        --arch="$ARCH" \
        --cpu="$CPU" \
        --cc="$CC" \
        --cxx="$TOOLCHAIN/bin/${TARGET}${API}-clang++" \
        --ar="$TOOLCHAIN/bin/llvm-ar" \
        --nm="$TOOLCHAIN/bin/llvm-nm" \
        --strip="$TOOLCHAIN/bin/llvm-strip" \
        --sysroot="$SYSROOT" \
        "${COMMON_CONFIG[@]}"
    then
        echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
        echo "!!! CONFIGURE FAILED! Dumping ffbuild/config.log for $ABI: !!!"
        echo "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!"
        cat "$BUILD_DIR/ffbuild/config.log"
        exit 1
    fi

    echo ">> Building..."
    make -j"$(nproc)"

    echo ">> Installing to $INSTALL_DIR"
    make install

    echo "✅ Finished: $ABI"
    echo
}


build_ffmpeg_for_abi arm64-v8a
build_ffmpeg_for_abi x86_64