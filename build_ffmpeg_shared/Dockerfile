FROM ubuntu:22.04 AS builder

RUN apt-get update && \
    apt-get install -y \
        git wget unzip curl build-essential yasm pkg-config ninja-build nasm && \
    rm -rf /var/lib/apt/lists/*

ENV NDK_VERSION=r27d
ENV ANDROID_NDK=/opt/android/android-ndk-${NDK_VERSION}

RUN mkdir -p /opt/android && \
    cd /opt/android && \
    wget https://dl.google.com/android/repository/android-ndk-${NDK_VERSION}-linux.zip && \
    unzip android-ndk-${NDK_VERSION}-linux.zip && \
    rm android-ndk-${NDK_VERSION}-linux.zip

WORKDIR /src
RUN git clone --depth=1 https://github.com/FFmpeg/FFmpeg.git ffmpeg

COPY build-ffmpeg.sh /scripts/build-ffmpeg.sh
RUN chmod +x /scripts/build-ffmpeg.sh

RUN /scripts/build-ffmpeg.sh

FROM ubuntu:22.04 AS final

COPY --from=builder /build /build
