Day 07

## 作业1：

### Part 1

10分：Android HelloWorld工程上传git

- 路径 => `./helloWorld/`

### Part 2

10分：签名包上传git，查看包内资源分布，截图上传

本地创建一个签名文件 => `./signature.jks`

然后直接用 Android Studio 来 build 即可，analyze 生成的 apk 包内资源分布如下：

![part1](assets/part1.png)

可以看到占主要大小的是 `classes.dex`，也即 Java 类编译后的产物

此外还有：

- `res`：二进制资源文件
- `assets`: 原始资源文件
- `META-INF`: 元信息

等等

## 作业2

### part1

10分：用adb命令安装课上编译好的apk到手机中上传截图即可

首先是打开开发者模式，省略。然后使用 USB 设备 本地连接

```powershell
❯ adb devices
List of devices attached
10AD2P20UE00286 device
emulator-5554   device
```

使用 adb 安装

![adb install](assets/adbInstall.png)

安装到手机后运行结果如下

> ~~手机上有没有好用的拼图软件推荐...我甚至得拿 magick 手动拼图~~

![result 01](assets/result01.jpg)

### part2

课后作业：编写一个 SO库，支持两个方法

20分：stringToFromJNI(返回一个字符串显示到界面
20分：add(int a,intb)做计算返回一个int值显示到界面
10分：应用app工程上传git库
10分：C++工程上传到git库
10分：提供截图

