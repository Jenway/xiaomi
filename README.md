# 大作业


## Day1 log

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

## Day2 log

需要使用三个线程，使用第一天编写队列，实现视频播放，无需实现音频

1. 实现 android 下使用 OpenGL ES 显示视频帧模块，可以正常看到画面，无花屏等情况
2. 播放器支持播放暂停
3. 播放器支持拖动，可以正常跳转，无花屏情况


现在的流程是：`Demuxer + Decoder` -> Mp4 parser 往 VideoFrameRender 的环形缓冲区里提交

环形缓冲区的实现复用了 Day 1 所述并发队列

然后 java 层借助 Choreographer 来定时触发 render 的 paint 

如果 C++ Native 层没有时钟的话那播放速度就和解码速度一样了，所以 C++ Native 层 Render 自己要维护一个时间戳，因此 Decoder 那边得把 pts 转格式后发过来，Render 根据时间戳来判断

上传视频：位于仓库根目录：`Day2.mkv`

## Day3 log

音频，让音频那边的时钟为主，在音频render 的回调里，更新 nativeplayer 里的主时钟，根据主时钟和 video 的 pts 来判断丢帧、等待还是播放，这样保证一个同步

上传视频：位于仓库根目录：`Day3.mkv`


