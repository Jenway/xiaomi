Day 09

## 作业要求

- 作业1: 10分
    - 编译 android 可用的 ffmpeg，将脚本上传到 gitlab
- 作业2: 20分
    - ffmpeg整体流程图，流程图中标注用到的函数
- 作业3: 15分
    - 解析下载的 mp4 文件，打印出 AVPacket 中的 pts 和 dts 的值
- 作业4: 15分
    - 解码 AVPacket 后的AVFrame，打印出其中宽高
- 作业5 40分
    - android 将 mp4 文件转换成YUV格式，并给出转换后产物上传，并上传用ffplay播放yuv的命令


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

### ffmpeg 调用的封装

首先来用 C++ 封装一个 Mp4Parse 类，实现作业 3、4、5 的功能，用于解析 MP4 视频文件，支持以下功能：

#### 1. 打开视频文件并初始化解码器（Demuxer + Decoder 初始化）

* **函数**：构造函数
* **关键步骤**：
  * 使用 `avformat_open_input` 打开媒体文件
  * 使用 `avformat_find_stream_info` 解析媒体流信息
  * 使用 `av_find_best_stream` 选择最佳视频流
  * 使用 `avcodec_find_decoder` + `avcodec_alloc_context3` 初始化解码器
  * 使用 `avcodec_open2` 打开解码器


#### 2. 读取压缩视频数据包（Demuxer）

* **接口**：`readNextVideoPacket(PacketInfo& info)`
* **主要调用**：`av_read_frame`
* **说明**：
  * 从视频流中读取一个 `AVPacket`（压缩帧）
  * 提取并返回该帧的 `PTS`、`DTS`、`size`、`flags` 等信息

**对应作业 3**：打印 `AVPacket` 中的 `PTS` / `DTS`

#### 3. 解码视频帧（Decoder）

* **接口**：`decodeNextVideoFrame(FrameInfo& frameInfo)`
* **主要调用**：`avcodec_send_packet` → `avcodec_receive_frame`
* **说明**：
  * 向解码器发送一个 `AVPacket`
  * 读取解码输出 `AVFrame`
  * 获取帧的 `宽度`、`高度`、`格式`、`pts`

**对应作业 4**：打印每个 `AVFrame` 的宽高等信息

#### 4. 导出视频为 YUV420P 文件（转换 + 写文件）

* **接口**：`dumpAllFramesToYUV(const std::string& path)`
* **说明**：
  * 解码：与前面一致，还是先读取为 AVPacket 再解码为 AVFrame
  * 转换：使用 `sws_getContext` 创建转换上下文，目标格式为 `AV_PIX_FMT_YUV420P`（通过设置 dst AVFrame 的 format 字段）
  * 使用 `sws_scale` 将帧统一转为 `YUV420P`
  * 将每帧的 Y、U、V 分量逐一 fwrite 到输出文件，得到 `.yuv` 裸视频数据

> 虽然没有真正用 FFmpeg 的 `Encoder/Muxer`，但也可看作一种简化的编码输出过程。

#### CPP 类 overview

```cpp
class Mp4Parser {
public:
    bool open(const std::string& filename);     // 初始化并打开解码器
    bool readNextVideoPacket(PacketInfo& info); // 提取 AVPacket 的信息
    bool decodeNextVideoFrame(FrameInfo& info); // 提取 AVFrame 的信息
    bool dumpAllFramesToYUV(const std::string& path); // 转换导出为 YUV
    ~Mp4Parser();                               // 自动释放资源
};
```

### ffmpeg 调用的封装的 JNI 封装

为了让 Android 应用通过 Java 调用上一节描述的功能，通过 JNI 封装前面实现的 `Mp4Parser` 类。

#### Native 层实现

在 `mp4_parser_jni.cpp` 中实现 JNI 接口，每个 native 函数对应 `Mp4ParserJNI` 中的一个方法。例如：

* `open(String)` → 初始化并打开 `Mp4Parser`
* `readNextVideoPacket()` → 读取一个压缩包并转换为 `PacketInfo` Java 对象
* `decodeNextVideoFrame()` → 解码一个视频帧并转换为 `FrameInfo`
* `dumpAllFramesToYUV(String)` → 写入 `.yuv` 文件
* `close()` → 调用 `Mp4Parser::close()` 并释放内存

JNI 中还实现了辅助函数：

* `setMp4Parser(JNIEnv*, jobject, Mp4Parser*)`：保存 native 指针到 Java 对象字段
* `getMp4Parser(JNIEnv*, jobject)`：获取 native 指针

#### nativeHandle 的保存方式说明

在 Java 类中使用 `private long nativeHandle;` 字段，在 C++ 中配合 `setMp4Parser()` / `getMp4Parser()` 方法进行存取（通常通过反射字段 ID 设置），从而将 C++ 对象生命周期与 Java 对象绑定。

例如：

```cpp
// jni_helpers.hpp
inline void setMp4Parser(JNIEnv* env, jobject obj, Mp4Parser* parser);
inline Mp4Parser* getMp4Parser(JNIEnv* env, jobject obj);
```

---

#### 封装结构总览

```java
// Java 层
Mp4ParserJNI
 ├── open(String)
 ├── readNextVideoPacket() → PacketInfo
 ├── readAllVideoPackets() → List<PacketInfo>
 ├── decodeNextVideoFrame() → FrameInfo
 ├── dumpAllFramesToYUV(String)
 └── close()
```

```cpp
// Native 层
Mp4Parser
 ├── open()
 ├── readNextVideoPacket()
 ├── readAllVideoPackets()
 ├── decodeNextVideoFrame()
 ├── dumpAllFramesToYUV()
 └── close()
```

### 构建产物

```bash
cd ffmpegJNI
bash build.sh
```

> 为了方便 debug，将 `src`  和 `test`的构建分开，前者链接 Part 1 交叉编译的 ffmpeg 动态库，后者编译本地安装的 ffmpeg 库。
>
> 将 Mp4Parser C++ 类单独构建为一个静态类 `libmp4parser_core.a`，分别和 test 或 src 链接
> 
> run `bash test.sh` 构建测试程序

那么 build 完就是这样的

![build ffmpeg JNI](assets/buildJNI.png)

文件结构大概这样，具体以仓库为准吧

```bash
❯ exa -T . -L 2
.
├── build.sh
├── build_android
│  ├── arm64-v8a
│  └── x86_64
├── CMakeLists.txt
├── compile_commands.json -> build_android/arm64-v8a/compile_commands.json
├── include
│  ├── jni_helpers.hpp
│  ├── mp4_parser.hpp
│  └── mp4_parser_jni.hpp
├── output.mp4
├── output.yuv
├── src
│  ├── CMakeLists.txt
│  ├── jni_helpers.cpp
│  ├── jni_on_load_unload.cpp
│  ├── mp4_parser.cpp
│  └── mp4_parser_jni.cpp
├── test
│  ├── CMakeLists.txt
│  └── test_main.cc
└── test.sh
```


## Part 4

太长了所以我分个章节

首先还是创建一个安卓项目把前述的

- 交叉编译的 ffmpeg 动态库
- ffmpegJNI库也就是 `libmp4parser.so`

按照架构放到 `src/main/jniLibs` 里

然后还要再写一个 ffmpeg 调用的封装的 JNI 封装的 JAVA 封装

那么调用层次大概是这样的

``` 
Android app ->
java JNI Wrapper（com.example.ffmpeg ->
cpp JNI wrapper（libmp4parser.so）->
cpp wrapper of ffmpeg lib（Mp4Parser） ->
ffmpeg/lib/*.so
```

### Java 类设计

* **包名**：`com.example.ffmpeg`
* **类名**：`Mp4ParserJNI`
* **作用**：作为 JNI native 方法的桥接类，封装 `Mp4Parser` 的功能。

```java
public class Mp4ParserJNI {
    static {
        System.loadLibrary("avutil");   // libavutil.so
        //  。。。。以及一堆 ffmpeg 的 so
        System.loadLibrary("mp4parser"); // 加载 native 库
    }

    // native 句柄管理
    private long nativeHandle;

    public static class PacketInfo { /*略*/}
    public static class FrameInfo { /*略*/}

    // 打开 MP4 文件
    public native boolean open(String filePath);
    // 读取下一个压缩视频包（Packet）
    public native PacketInfo readNextVideoPacket();
    // 一次性读取所有压缩视频包
    public native java.util.List<PacketInfo> readAllVideoPackets();
    // 解码下一帧视频
    public native FrameInfo decodeNextVideoFrame();
    // 将所有帧导出为 YUV 格式文件
    public native boolean dumpAllFramesToYUV(String yuvOutputPath);
    // 关闭并释放资源
    public native void close();
}
```

### Android app 设计

### 结果

- 作业3: 15分
    - 解析下载的 mp4 文件，打印出 AVPacket 中的 pts 和 dts 的值

![03](assets/image.png)

- 作业4: 15分
    - 解码 AVPacket 后的AVFrame，打印出其中宽高
    - 一般来说他们的宽高都一样的，所以这里是一个一个点的

![04](assets/image-1.png)

- 作业5 40分
    - android 将 mp4 文件转换成YUV格式，并给出转换后产物上传，并上传用ffplay播放yuv的命令

 ![05](assets/image-3.png)

然后用 adb pull 出来

```bash
❯ ffplay -f rawvideo -pixel_format yuv420p -video_size 1920x1080 output.yuv
```

![05](assets/image-4.png)

