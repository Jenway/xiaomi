# 大作业

Android OpenGLES 播放器

- 基础功能
    1. 将视频解封装成**音频流**和**视频流**
    2. 将解码后的音频流和视频流解码成**音频帧**和**视频帧**
    3. 编写 OpenGLES 模块显示解码后的视频数据（**同步**调用）
    4. 利用提供的 AAudio 播放解码后的音频数据（**异步**调用）
    5. 播放与暂停
    6. **音视频同步**
- 扩展功能
    1. ~~倍速播放，需注意如何保证**音频播放速度**的改变的同时**不改变音调**~~
    2. ~~获取**视频信息**，如宽高，时长，编码格式~~
    3. 进度跳转：非精确 seek，~~精确 seek~~

## 基本架构

Android 层面由于已经给好了框架所以不用说了，只说 JNI 以下的

Demuxer 从视频文件中（调用 FFmpeg）API 来读 Packet ，发送到一个线程安全队列中，Decoder 从里面读（阻塞读）

Decoder 把 Packet 解码为 Frame 发送到 Frame 队列中，然后

- 对于音频，我们注册回调函数，当回调函数来调用我们，我们就帮它从 Audio Frame 中读 Frame 并返回给它，在这个回调中需要更新时钟
- 视频 Decoder 自己在一个单独线程里，它持有一个上述阻塞队列
- 当我们的时钟判断合适时就 submit 给视频 decoder 让它解码

> 看起来视频 Decoder 也没必要留一整个队列啊？
>
> 这是历史遗留问题，因为 Day2 的时候不需要考虑音频，这个时候时钟不在 NativePlayer 而是在 Decoder 那边，也就 Frame submit 给 Decoder 内置队列，让它自己来选择什么时候调用

## 组件

### 线程安全的队列 SemQueue

> 安全应该基本安全，至于性能，就别指望太多了。而且真要考虑性能的话肯定还是上各种无锁队列比如 `boost::lockfree::spsc_queue` 这种吧

`SemQueue` 是一个基于信号量机制实现的线程安全的队列适配器模板类。其核心通过两个计数信号量（slots）管理队列的“空位”与“已填充元素”，从而保证并发访问的正确性。

* `empty`：表示当前可写入的槽位数，初始化为队列容量。
* `filled`：表示当前可读取的元素数，初始化为 0。

操作流程如下：

* **生产者写入前**，必须先 `acquire` 一个 `empty` 槽，确保队列有空间；
* **写入完成后**，调用 `release` 向 `filled` 发出可读通知；
* **消费者读取前**，必须先 `acquire` 一个 `filled` 槽，确保队列非空；
* **读取完成后**，再 `release` 一个 `empty`，腾出空间供写入。

该机制避免了经典的并发写入竞态问题：
比如当队列剩一个槽位时，如果两个生产者线程几乎同时判断“队列未满”并写入，就可能导致数据冲突。而通过信号量控制，只有成功获取 `empty` 的线程才能继续写入，从根本上避免了这种冲突。

#### 信号量实现

我们自定义了一个简化版计数信号量类 `Semaphore`，其接口设计参考 `std::counting_semaphore`，使用 `std::mutex` + `std::condition_variable` 实现，支持阻塞与非阻塞获取：

* `void acquire();`：阻塞获取
* `bool try_acquire();` 非阻塞
* `bool try_acquire_for(std::chrono::duration);` 一段给定时间内阻塞
* `void release();`
* `void release_all();` 可用于广播唤醒，例如在前述队列的 shutdown() 操作中让所有阻塞线程立即返回。

#### 环形缓冲区：RingBuffer

我们将线程安全的任务交给前述 SemQueue，RingBuffer 只要实现简单接口即可，

* 使用一个固定大小的数组；
* 用两个游标维护读/写位置；
* 保持 `size` 信息用于检查状态。

所以它被用的时候就是这样的

```cpp
std::unique_ptr<SemQueue<T, RingBuffer<T, Size>>> queue = 
    std::make_unique<SemQueue<T, RingBuffer<T, Size>>>(Size);
```

> auto + IDE 提示 最好用的一集

___

### FFmpeg 单 so

简单来说，FFmpeg 官方脚本并不支持直接将所有模块打包成一个动态库，并且它们被拆分为多个 `libav*.so` 是有合理性的 —— 这些库的底层符号空间很容易发生冲突。

因此退而求其次，我们选择编译成静态库，再统一链接为一个 .so。不过由于 CMake 不允许没有源文件的目标参与构建，因此添加了一个 dummy 源文件。最终把它和我们自己的 JNI 库一并打包成一个 .so。

那么还是和之前某个作业一样，写个 Dockerfile 来编译，基本没什么要改的...吗？

> 我不会告诉你我花了一个小时研究怎么把几个动态库合并成一个，然后才恍然大悟根本不行……
>
> 后来发现一开始还把 AAC 解码编译给禁用了，又重新编译了一遍……

```
❯ exa -T build_ffmpeg_shared -L 3
build_ffmpeg_shared
├── build-ffmpeg.sh
├── build.sh
├── Dockerfile
└── out
   ├── arm64-v8a
   │  ├── include
   │  ├── lib
   │  ├── lib_static
   │  └── share
   └── x86_64
      ├── include
      ├── lib
      ├── lib_static
      └── share
```

### Demuxer & Decoder

`ffmpegJNI` 路径下，由于它们关系很近（都调 `FFmpeg` api），所以它们被一个叫 Mp4Parser 的类持有

```bash
❯ exa -T ffmpegJNI/include -L 3
ffmpegJNI/include
├── Decoder.hpp
├── DecoderContext.hpp
├── Demuxer.hpp
├── MediaSource.hpp
├── Mp4Parser
│  └── FrameProcessor.hpp
├── Packet.hpp
└── YuvFileSaver.hpp
❯ exa -T ffmpegJNI/src -L 3
ffmpegJNI/src
├── CMakeLists.txt
├── Decoder.cc
├── Demuxer.cc
├── Mp4Parser.cc
└── utils
   ├── AudioFrame.cc
   ├── DecoderContext.cc
   ├── FrameProcessor.cc
   ├── MediaSource.cc
   ├── Packet.cc
   └── ParserUtils.cc
```

#### Demuxer

从上层获得 `AVFormatContext *` 调用 `av_read_frame` ，得到 Packet ，再用这个 Packet 写 `PacketSink` 回调即可

> 本来设计 PacketSink 回调接口是为了方便写单元测试，结果后面太忙就咕了 hh

Mp4Parser 传递给它这样一个 callback：根据 streamidx push 到 VideoPacketQueue 或者 AudioPacketQueue

>啊，看起来多么简洁——如果不用管各种暂停停止甚至还有 seek 的话

#### Decoder

它同样接收一个 callback，

然后他就会调用完 avcodec_send_packet(ctx, packet) 然后 avcodec_receive_frame(ctx_->get(), decoded_frame_)后使用 frame 调这个回调函数

在这个项目里这个 sink 就是：写到 VideoFrameQueue / AudioFrameQueue 里

> 啊，看起来... 额这个确实受 seek 摧残少一点hh

值得注意的是，FFmpeg 解出的 frame 通常不能直接用于渲染：

- 视频帧需要转码为 YUV；
- 音频帧要从 planar 格式（AV_SAMPLE_FMT_FLTP）转换为 interleaved 格式（AV_SAMPLE_FMT_S16）才能送给 AAudio。

这些逻辑统一封装在了 `FrameProcessor.cc` 中。

#### Mp4Parser

> 叫这个名字是因为一开始想复用之前实验的代码，结果并没有派上多少用场。

它持有 Demuxer 、Packet Queue 以及 PipeLine（Decoder & FrameQueue）并统筹调用他们

它采用了有限状态机（FSM）的设计思路，维护 START、STOP、PAUSE、RESUME、SEEK 等状态。最大的好处是调用时更简单，不需要外层再频繁加锁判断状态，换句话说就是将状态判断的心智负担内聚到了类内部。

### Render

Audio Render 主要是调给定的实现，这里不说了

Video Render 和之前某天作业也差不多，只不过那天的作业给的框架封装了 EGL 调用，所以我们有 `EGL core ` 类来从 `ANativeWindow` 中绑定 surface 、make context，然后 OpenGL 的绘制就写到 EGL 的backbuffer，最后交换buffer 并回收上下文即可

OpenGL 调用部分集中在 `GLESRender` 中。相比之前的作业，这里更简单：

只需渲染一张全屏图像，无需设置顶点位置或进行变换，shader 相应也更精简。

如果要实现扩展功能（水印），可以借鉴前几天实现前景/背景图片叠加的作业 —— 大致思路是用前景后景绘制两次实现叠加效果。

那么也有 `glRenderHolder` 来统筹两个组件，他也是个状态机，额，曾经是？正如前面所说，他本来是还管理时钟，而且自己决定要不要 render 的，但是为了音视频同步，它现在基本成了个被动渲染的类了，主要调用它的 `submit_frame` 进行渲染

```bash
❯ exa -T videoFrameRender -L 3
videoFrameRender
├── CMakeLists.txt
├── include
│  ├── EGLCore.hpp
│  └── GLESRender.hpp
└── src
   ├── EGLCore.cc
   ├── GLESRender.cc
   └── GLRenderHost.cc
```

### NativePlayer

它统筹 Mp4Parser 和 Renders，同时还管理着 master 时钟

简单来说就是以音频那边的时钟为主，在音频render 的回调里，更新 nativeplayer 里的主时钟，根据主时钟和 video 的 pts 来判断丢帧、等待还是播放，如果播放就提交给 render

它也是个状态机

它处理各种生命周期，状态相应、回调 blabla

> 我也不想写这么大的类，但是谁一开始不想把东西都写的小而美呢hh

``` bash
❯ exa -T common -L 3
common
├── include
│  ├── AAudioRender.h
│  ├── AudioFrame.hpp
│  ├── Entitys.hpp
│  ├── GLRenderHost.hpp
│  ├── Mp4Parser.hpp
│  ├── NativePlayer.hpp
│  ├── RingBuffer.hpp
│  ├── Semaphore.hpp
│  └── SemQueue.hpp
└── src
   └── NativePlayer.cc
```

### Seek

实现的是最简单的 `非精确 seek`：

调用 FFmpeg 的 `av_seek_frame` 定位到一个最近的关键帧，然后重新开始 Demux -> Decode 流程。seek 后需手动 flush PacketQueue 和 FrameQueue，以清除过时数据。

> 挺简单的...吗？给我整崩溃了，我甚至觉得 LLM 要是有精神估计也错乱了

### pimpl

有些组件比如 NativePLayer 用了这种方法，对于我来说一个好处就是， pimpl 允许头文件里少 include，譬如我可以提供一个调用了 OpenGL 的 api 的库的头文件，但是里面没有 include OpenGL api，而是在源文件再 include ，这样只需要提供一个 so 和一个头文件，不需要对方有 OpenGL 依赖

我个人比较喜欢将 JNI 项目与 Android 项目物理分离，因此最后会将 .so 文件和头文件单独拷贝到 Android 项目目录中。

效果就是：只需复制少量头文件与一个 .so 文件到 Android 项目即可，无需额外依赖 FFmpeg 或 OpenGL 的开发头文件，非常方便。

``` bash
❯ exa -T androidplayer/app/src/main/cpp -L 3
androidplayer/app/src/main/cpp
├── CMakeLists.txt
├── include
│  ├── Entitys.hpp
│  ├── GLRenderHost.hpp
│  ├── Mp4Parser.hpp
│  └── NativePlayer.hpp
└── PlayerJNI.cpp
❯ exa -T androidplayer/app/src/main/jniLibs -L 3
androidplayer/app/src/main/jniLibs
├── arm64-v8a
├── armeabi-v7a
└── x86_64
   └── libffmpeg-jw.so
```

## log ，也就是前几日的 README

### Day1 log

要求：

1. 使用 **android ndk** 编译 ffmpeg 并集成到项目中（**单个 so** 的形式，命名为 **libffmpeg-xxx.so**）
2. 编写**队列代码**，并自行验证正确性
3. 将视频解码成**视频帧(YUV)**，并保存到文件，使用 ffplay 检查正确性，并**上传录屏**

- 交叉编译 ffmpeg，还是用的 docker ，在 `build_ffmpeg_shared` 路径下，和之前实验差不多，只是又开启了静态库编译选项
- 编写 demuxer 和 decoder：在 `ffmpegJNI` 路径下
    - 分为 mp4parser_core 部分和对外暴露接口部分
    - 把 ffmpeg 的那些静态库也打进去，最终会输出一个 `libffmpeg-jw.so`，完成 `一个` so的要求

那么 log 一下 frame 信息

![alt text](assets/image.png)

由于把 mp4parser_core 和 AndroiPlayer C++ Native 层分开了，主要是在 Android 这边还要读写文件 adb pull 出来，所以我就在主机这边直接转换 yuv 了，反正用的都是一个 mp4parser_core 静态库

上传视频：位于仓库根目录：`Day1.mkv`

哦对了，还有并发线程安全队列代码的正确性，`ffmpegJNI/test/` 路径下有一些测试，其中 `test_queue.cc` 测试了这个队列，实现思路和 Day 5 lab 差不多

### Day2 log

需要使用三个线程，使用第一天编写队列，实现视频播放，无需实现音频

1. 实现 android 下使用 OpenGL ES 显示视频帧模块，可以正常看到画面，无花屏等情况
2. 播放器支持播放暂停
3. 播放器支持拖动，可以正常跳转，无花屏情况


现在的流程是：`Demuxer + Decoder` -> Mp4 parser 往 VideoFrameRender 的环形缓冲区里提交

环形缓冲区的实现复用了 Day 1 所述并发队列

然后 java 层借助 Choreographer 来定时触发 render 的 paint 

如果 C++ Native 层没有时钟的话那播放速度就和解码速度一样了，所以 C++ Native 层 Render 自己要维护一个时间戳，因此 Decoder 那边得把 pts 转格式后发过来，Render 根据时间戳来判断

上传视频：位于仓库根目录：`Day2.mkv`

### Day3 log

音频，让音频那边的时钟为主，在音频render 的回调里，更新 nativeplayer 里的主时钟，根据主时钟和 video 的 pts 来判断丢帧、等待还是播放，这样保证一个同步

上传视频：位于仓库根目录：`Day3.mkv`


