Day 07

## 作业1：

### Part 1

**10分：Android HelloWorld 工程上传 git**

- 路径 => `./helloWorld/`

### Part 2

**10分：签名包上传git，查看包内资源分布，截图上传**

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

**10分：用adb命令安装课上编译好的apk到手机中上传截图即可**

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

- 20分：`stringToFromJNI()` 返回一个字符串显示到界面
- 20分：`add(int a,intb)` 做计算返回一个int值显示到界面
- 10分：应用 app 工程上传 git 库
- 10分：C++ 工程上传到 git 库
- 10分：提供截图

___

首先创建一个 `Native C++` 项目，实现这两个函数即可

- 路径 => `./myLib/`

```cpp
extern "C" JNIEXPORT jstring JNICALL
Java_com_example_mylib_stringToFromJNI(JNIEnv *env, jobject /* this */) {
    std::string hello = "Hello from JNI calling C++";
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT jint JNICALL
Java_com_example_mylib_add(JNIEnv *env, jobject /* this */, jint a, jint b) {
    return a + b;
}
```

在 `./mylib` 中 run `./gradlew :mylib:externalNativeBuildDebug`

编译得到产物

```bash
.../myLib/mylib/build/intermediates/cmake/debug/obj
❯ eza -T
.
├── arm64-v8a
│   └── libmylib.so
├── armeabi-v7a
│   └── libmylib.so
├── x86
│   └── libmylib.so
└── x86_64
    └── libmylib.so
```

新建一个 Android 应用项目，创建一个 `app/src/main/jniLibs` 目录，将上述动态库复制到里面

- 路径 => `./jniRunner/`


增加一个 java 类来提供接口声明

``` java
package com.example;
public class mylib {
    static {
        System.loadLibrary("mylib");
    }
    public static native String stringToFromJNI();
    public static native int add(int a, int b);
}
```

然后调用函数实现一个界面即可，编译运行结果如下：

![JNI Result](assets/jniResult.png)

## 文件结构

杂七杂八太多了 eza 也不管用了我手写得了

- `helloWorld/app/src/main`
  - `java/com/example/helloworld/MainActivity.java`：应用入口
  - `res/layout/activity_main.xml` 修改了一下 XML 样式，也就是修改了一下 helloworld 的内容
  - `res/values/strings.xml`：同上

然后是 `myLib`

- `myLib\mylib\src\main\cpp\native-lib.cpp`
- `myLib\mylib\src\main\cpp\CMakeLists.txt`

也就是两个函数的实现以及 `CMakeLists`，外部的 gradle 会调用 CMake 进行编译然后产出动态库

还有 `jniRunner/app/src/main/`

- `jniLibs`
  - 上个项目的输出动态库产物
- `java/com/example/mylib.java`
  - 用来给出调上述动态库的接口类，要和上个项目里定义的一致
- `java/com/example/jnirunner/MainActivity.kt`
  - 应用的主文件，简单调用并展示两个函数
  