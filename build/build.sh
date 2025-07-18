#!/bin/bash
set -e

# ❯ docker run --rm -v $PWD/out:/out ffmpeg-builder bash -c "cp -r /build/android/* /out/"
# ❯ docker build --progress=plain -t ffmpeg-builder .

sudo rm out -rf

docker build -t ffmpeg-builder . && docker run --rm -v "$PWD/out:/out" ffmpeg-builder cp -r /build/android/. /out/
