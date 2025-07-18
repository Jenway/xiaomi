Day 09

## 作业要求

- 作业1: 10分
    - 编译 android 可用的 ffmpeg，将脚本上传到 gitlab
- 作业2: 20分
    - ffmpeg整体流程图，流程图中标注用到的函数
- 作业3: 15分
    - 解析下载的mp4文件，打印出AVPacket中的pts和dts的值
- 作业4: 15分
    - 解码AVPacket后的AVFrame，打印出其中宽高
- 作业5 40分
    - android将mp4文件转换成YUV格式，并给出转换后产物上传，并上传用ffplay播放yuv的命令


## Part1

编译 android 可用的 ffmpeg，这里使用 Docker 来构建

```bash
cd build && bash build.sh
```

Dockerfile 主要干这几件事

- 安装必要依赖
    - 下载 Android NDK 库（版本 r27d）
    - 下载 FFmpeg 源码（版本 7.1.1）
    - 以及若干其他依赖，比如（YASM、NASM等等）
- 运行编译脚本 `build/scripts/build-ffmpeg.sh`
    - 这个脚本会调用 FFmpeg 源码构建

### Misc

- DNK 新版的比如 nm 这些他们的 架构前缀去掉了，所以需要修改，
- CPU 架构要写 arm-v8a 和 x86-64，否则会错

### 文件结构

``` bash
❯ exa -T build -L 3
build
├── build.sh
├── Dockerfile
├── out
│  ├── arm64-v8a
│  │  ├── include
│  │  ├── lib
│  │  └── share
│  └── x86_64
│     ├── include
│     ├── lib
│     └── share
└── scripts
   └── build-ffmpeg.sh
```

> 当然， out 路径直接放到 `.gitignore` 里，不上传到操控

## Part2

ffmpeg 整体流程图，流程图中标注用到的函数

1. Demuxer（解复用器）
    - 从 MP4、MKV 等容器格式中提取音视频数据流（如 h264、aac）
2. Decoder（解码器）
    - 把压缩后的码流（AVPacket）解码成原始帧（AVFrame）
3. Filter（滤镜器）
    - 图像/音频滤镜处理，如缩放、裁剪、加字幕、降噪等
4. Encoder（编码器）
    - 将处理后的 AVFrame 编码为压缩数据（AVPacket）
5. Muxer（复用器）
    - 将音视频码流（AVPacket）封装进媒体容器（如 MP4、FLV）

![part 2](assets/ffmpeg.drawio.png)

## Part3

- 作业3:15分
    - 解析下载的mp4文件，打印出AVPacket中的pts和dts的值
- 作业4:15分
    - 解码AVPacket后的AVFrame，打印出其中宽高
- 作业5:40分
    - android将mp4文件转换成YUV格式，并给出转换后产物上传，并上传用ffplay播放yuv的命令

