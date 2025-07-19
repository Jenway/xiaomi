## Day 10

### 课堂练习1

**任务**：  
a. 实现交通信号灯有限状态机，包含红（Red）、绿（Green）、黄（Yellow）三种状态。  
b. 状态转换规则：  
   - 红→绿（10时间单位）  
   - 绿→黄（8时间单位）  
   - 黄→红（2时间单位）  
c. 附加功能：通过输入控制“开关机”（切换状态逻辑）。  

**评分点（20分）**：  
a. 正确实现状态转移。  
b. 使用多线程或系统中断实现开关机功能（额外加分）。  
c. 提交代码及运行截图。  

![01](assets/1.png)

___

目录：`./traffic`

```bash
❯ exa -T
.
├── build
│  ├── compile_commands.json
│  └── traffic
├── include
│  └── TrafficLight.hpp
├── Makefile
├── obj
│  ├── main.d
│  ├── main.o
│  ├── TrafficLight.d
│  └── TrafficLight.o
└── src
   ├── main.cc
   └── TrafficLight.cc
```

### 课后作业2

**任务**：  

a. 把以下图片绘制到界面上  

**评分点（80分）**：  

a. 图片显示正常。  
b. 提交代码及运行截图。

### 思路

可以分为两部分

- Decoder：把图片解码为 RGBA buffer
- Display：显示这个 buffer

由于这个图片实际上是个 jpeg，所以 Decoder 使用的 [libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo)

封装 libjpeg-turbo 的 api 实现一个符合 JpegDecoder 接口的类，也就是实现一个`ImageData decode(const std::string &filepath)` 方法

返回一个结构体，包含着一个 RGB buffer

```cpp
struct ImageData {
    std::vector<unsigned char> pixel_data;
    unsigned int width = 0;
    unsigned int height = 0;
    int channels = 0; // 3 bydefault
};
```

pixel_data 这样的结构可以直接交给某些图形API，譬如用 SDL

``` cpp
SDL_UpdateTexture(texture, nullptr, image.pixel_data.data(), image.width * image.channels);
```

![02](assets/0202.png)

或者 OpenGL

```cpp
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.width, image.height, 0, GL_RGB, GL_UNSIGNED_BYTE, image.pixel_data.data());
 ```

![02](assets/image.png)

### 运行

编译的话要装一堆依赖，可惜我忘记记下来了

run `bash build.sh`

然后 run `./build/bin/JpegReaderSDL static/logo.jpg` 或者 `./build/bin/JpegReaderOpenGL static/logo.jpg`

目录：`./jpegReader`


```
❯ exa -T . -L 3
.
├── build
├── build.sh
├── CMakeLists.txt
├── include
│  ├── DisplayInterface.hpp
│  ├── JpegDecoder.hpp
│  ├── OpenGLDisplay.hpp
│  └── SDLDisplay.hpp
├── lib
│  ├── libjpeg-turbo
│  └── notcurses
├── src
│  ├── CMakeLists.txt
│  ├── JpegDecoder.cc
│  ├── opengl
│  │  ├── CMakeLists.txt
│  │  ├── main_OpenGL.cc
│  │  └── OpenGLDisplay.cc
│  └── sdl
│     ├── CMakeLists.txt
│     ├── main_SDL.cc
│     └── SDLDisplay.cc
├── static
│  └── logo.jpg
```
