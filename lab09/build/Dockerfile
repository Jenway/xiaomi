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

ENV FFMPEG_VERSION=7.1.1

WORKDIR /src
RUN wget https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.xz && \
    tar -xvf ffmpeg-${FFMPEG_VERSION}.tar.xz && \
    mv ffmpeg-${FFMPEG_VERSION} ffmpeg && \
    rm ffmpeg-${FFMPEG_VERSION}.tar.xz

COPY scripts/build-ffmpeg.sh /scripts/build-ffmpeg.sh
RUN chmod +x /scripts/build-ffmpeg.sh

RUN sed -i 's/^\(.*ff_aom_uninit_film_grain_params(&s->aom_film_grain);\)/ \/\/ \1/' /src/ffmpeg/libavcodec/h2645_sei.c

RUN /scripts/build-ffmpeg.sh

FROM ubuntu:22.04 AS final

COPY --from=builder /build /build
COPY --from=builder /src/ffmpeg/ /build/ffmpeg
