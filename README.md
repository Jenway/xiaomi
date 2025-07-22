


那么 log 一下 frame 信息

![alt text](assets/image.png)

由于把 mp4parser_core 和 AndroiPlayer C++ Native 层分开了，主要是在 Android 这边还要读写文件 adb pull 出来，所以我就在主机这边直接转换 yuv 了，反正用的都是一个 mp4parser_core 静态库


