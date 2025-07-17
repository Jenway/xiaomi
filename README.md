## Day

课后作业：
- 作业1：20分
    - 在课堂演示工程中编写makeFile，生成可执行文件，上传代码与截图
- 作业2：20分
    - 在课堂演示工程中编写CMakeLists.txt，分别生成可执行文件，生成动态库，生成静态库，上传代码与截图
- 作业3：10分
    - 编写简易数学运算工程（加减乘除）
- 作业-4：20分
    - 创建编译脚本，执行编译，生成Android可用动态库，提供（加减乘除）功能
- 作业5：30分
    - 创建Android工程，使用JNI接口，调用运算功能（加减乘除），显示到界面，类似于最基本的计算器功能
    - 要求：上传Android代码和so库源码代码，客户端显示截图


### 作业 1

那么 `01` 文件结构如下

``` bash
❯ exa -T
.
├── build
├── include
│  ├── add.h
│  └── sub.h
├── Makefile
├── obj
└── src
   ├── add.cc
   ├── calc.cc
   └── sub.cc
```

- 为了便于扩展的话，我们可以通配符 wildcard 来将 `src` 路径下所有源文件获取到 `SRCS` 变量中
- 我们统一把中间编译产物放到 `obj` 路径里，方便 make clean
- 我们使用 `$<` `$@` 定义一些通用规则来复用重复语句

那么核心就是下面的部分

```Makefile
# ... 设置编译器、源文件夹路径等等...
SRCS := $(wildcard $(SRC_DIR)/*.cc)
OBJS := $(patsubst $(SRC_DIR)/%.cc, $(OBJ_DIR)/%.o, $(SRCS))

all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cc | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@
# ... clean、fclean、re等等...
```

运行结果就是这样的

![Result of Make](assets/make.png)

### 作业 2

`02` 文件结构是这样的

```bash
❯ exa -T
.
├── build
├── CMakeLists.txt
├── include
│  └── math
│     ├── add.h
│     └── sub.h
├── lib
│  ├── add
│  │  ├── CMakeLists.txt
│  │  └── src
│  │     └── add.cc
│  └── sub
│     ├── CMakeLists.txt
│     └── src
│        └── sub.cc
└── src
   ├── CMakeLists.txt
   └── main.cc
```

假设我们写了一个库给别人用的话，一般会提供一个头文件和一个 `.a` 或 `.so` ，所以我们这里只需要有一份头文件，在统一的 include 目录下即可

add 是一个 Shared 库、sub 是一个静态库

对于库模块，我们放在 lib 下，每个库自己有编译规则，注意 include 应该置为 PUBLIC 才能传递给调用库的 src 中 executable

```CMake
add_library(math_add SHARED src/add.cc)
target_include_directories(math_add PUBLIC ${CMAKE_SOURCE_DIR}/include)
```

> sub 自然也类似

src 当然也有自己的规则

```CMake
add_executable(math_app main.cc)
target_link_libraries(math_app PRIVATE math_add math_sub)
```

最顶层的 CMakeLists.txt 把它们都作为子模块

设置一下编译标准、输出路径等等之后就可以运行了

run `cmake -Bbuild . && cmake --build build`

可以看到在指定位置生成了可执行文件、动态库和静态库

![Cmake Result](assets/cmake.png)


### 作业 3 & 4

这两个都在 `03` 目录

```bash
❯ exa -T
.
├── build.sh
├── CMakeLists.txt
├── include
│  └── math_utils.hpp
└── src
   └── math_utils_jni.cc
```

我们在 `math_utils.hpp` 里实现一个简单的加减乘除模板库即可，然而模板只有编译的时候才有，不可能让 JVM 现场编译 C++，所以得手动给它实例化以暴露接口，类似这样：

``` CPP
extern "C" {
    JNIEXPORT JniType JNICALL
    Java_com_example_mathutils_MathUtils_IntAdd(JNIEnv*, jobject, jint a, jint b) {
            return math_utils::add<int>(a,b);                                 
    }
}
```

`math_utils_jni.cc` 定义了一个宏来更方便的生成这种代码

在 CMakeists 中设置包含 Android NDK 的 include 和链接 Android NDK 的库

```CMake
# ...某些参数
add_library(math_utils_jni SHARED
    src/math_utils_jni.cc
)

target_include_directories(math_utils_jni PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    $ENV{ANDROID_NDK}/sysroot/usr/include
    $ENV{ANDROID_NDK}/sysroot/usr/include/android
)

find_library(log-lib log)

target_link_libraries(math_utils_jni
    ${log-lib}
)
```

使用下面的命令来构建即可

```bash
export ANDROID_NDK="somepath../android-ndk-r27d"

ANDROID_ABIS=("armeabi-v7a" "arm64-v8a" "x86" "x86_64")
ANDROID_PLATFORM=android-31
CMAKE_TOOLCHAIN_FILE="$ANDROID_NDK/build/cmake/android.toolchain.cmake"

BUILD_DIR="build/$ABI"

mkdir -p "$BUILD_DIR" && cmake -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE \
    -DANDROID_ABI=$ABI \
    -DANDROID_PLATFORM=$ANDROID_PLATFORM \
    -DCMAKE_BUILD_TYPE=Release \
    .

cmake --build "$BUILD_DIR" -- -j$(nproc)
```

run `bash build.sh`

![JNI build](assets/jniBuild.png)

那么之后就生成了产物 `libmath_utils_jni.so`

![JNI Result](assets/jniResult.png)

- arm64-v8a：可以简单理解为 64 位 ARM 设备
- 当然，之后作业 5 我直接用的本地模拟器，所以实际用的大概是 x86_64

### 作业 5

回到 Android Studio

> 又只能手写项目结构了hh

- `src/main/jniLibs`
    - 约定俗成的 jni lib 查找路径，分 ABI 存放共享库，作业 4 中编译出来的产物按照 ABI 复制在这里即可
- `src/main/java/common/example/mathutils`
    - `MathUtils` 类：对于作业四库的接口封装，也就是一些函数声明，例如：
        - `public static native int IntAdd(int a, int b);`
    - `MathWrapper.kt`：调用 `MathUtils` 的封装
        - 提供一个输入为 string，类型参数、操作参数的函数，dispatch 到对应操作上去，并提供了错误处理
- `src/main/java/common/example/myapplication`
    - `MainActivity.kt`：UI 逻辑的代码。

那么直接运行即可

![Android result](assets/android.png)

下面附有一个 GIF

![Android gif](assets/calculator.gif)


<video controls src="assets/calculator.mp4" title="Calculator"></video>

___

add cJson integrated to former net io

+ - * /