# 大作业


## Day1 log

- 交叉编译 ffmpeg，还是用的 docker ，在 `build_ffmpeg_shared` 路径下，和之前实验差不多，只是又开启了静态库编译选项
- 编写 demuxer 和 decoder：在 `ffmpegJNI` 路径下
    - 分为 mp4parser_core 部分和对外暴露接口部分
    - 把 ffmpeg 的那些静态库也打进去，最终会输出一个 `libffmpeg-jw.so`，完成`一个`so的要求

那么 log 一下 frame 信息

![alt text](assets/image.png)

由于把 mp4parser_core 和 AndroiPlayer C++ Native 层分开了，主要是在 Android 这边还要读写文件 adb pull 出来，所以我就在主机这边直接转换 yuv 了，反正用的都是一个 mp4parser_core 静态库


