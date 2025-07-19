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

#### 思路

那么，我们定义一个 `TrafficLight` 类，它内部通过一个独立线程运行交通灯状态机，负责循环输出红绿黄三种状态。主线程则负责处理用户输入（如开启、暂停、关闭）。

信号灯逻辑线程运行的伪代码类似：

```cpp
while (running) {
    switch (state) {
    case Red:
        print("Red");
        sleep(10);
        state = Green;
        break;
    case Green:
        print("Green");
        sleep(8);
        state = Yellow;
        break;
    case Yellow:
        print("Yellow");
        sleep(2);
        state = Red;
        break;
    }
}
```

为了实现通过输入控制灯的关停，我们将逻辑分为信号灯逻辑线程和用户输入线程（主线程）

主线程的逻辑的伪代码是这样的

```CPP
while (std::cin >> command) {
    if (command == "resume") light.resume();
    else if (command == "pause") light.pause();
    else if (command == "stop") light.stop();
}
```

可以理解为，交通信号灯应该是一个能暂停又再启动的函数，换句话说就是这个交通信号灯应该是个协程，当然这里没有必要用 C++20 coroutine，实际上我也不会用，只要让 TrafficLight 能提供类似 resume 和 pause 功能的接口即可

那么就需要保存几个状态

- 灯自身的状态（颜色） `std::atomic<TrafficLightState> current_state_`
- 是否暂停？`std::atomic<bool> is_cycling_paused_`
- 是否退出？`std::atomic<bool> run_logic_`

为了实现逻辑线程的暂停，引入条件变量，让逻辑循环在暂停时阻塞等待，直到被 resume() 唤醒。

虽然这些变量为原子类型，但为了配合保证状态切换与线程通信同步一致，还是有必要引入锁保护共享状态，况且条件变量也依赖于锁

总之：

- pause 函数会在加锁的情况下将暂停状态设为 true，触发逻辑循环的阻塞等待
- resume 函数同样会修改状态，然后 notify 条件变量使得循环继续执行

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

#### 思路

可以分为两部分

- Decoder：把 JPEG 图片解码为 RGB buffer
- Display：显示这个 buffer

由于这个图片实际上为 JPEG 格式，所以 Decoder 使用的 [libjpeg-turbo](https://github.com/libjpeg-turbo/libjpeg-turbo)

封装该库接口，设计一个 JpegDecoder 类，并实现如下接口，

``` CPP
ImageData decode(const std::string &filepath)
```

返回一个结构体，包含着一个 RGB buffer

```cpp
struct ImageData {
    std::vector<unsigned char> pixel_data; // RGB buffer
    unsigned int width = 0;
    unsigned int height = 0;
    int channels = 0; // 默认 3（RGB）
};
```

pixel_data 可直接传递给图形 API 进行显示。例如：

使用 SDL：

``` cpp
SDL_UpdateTexture(texture, nullptr, image.pixel_data.data(), image.width * image.channels);
```

![02](assets/0202.png)

或者 OpenGL

```cpp
glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, image.width, image.height, 0, GL_RGB, GL_UNSIGNED_BYTE, image.pixel_data.data());
 ```

![02](assets/image.png)

#### 运行

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
