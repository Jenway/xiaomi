cmake_minimum_required(VERSION 3.14)
project(FFmpegPlayerTests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 1. 定义 player_lib 的源文件 ---
# 你需要在这里列出构成播放器核心库的所有 .cc 文件。
# 路径是相对于当前 CMakeLists.txt (在 test/ 目录中) 的位置。
set(PLAYER_LIB_SOURCES
    ../src/Demuxer.cc
    ../src/utils/Packet.cc
    ../src/utils/MediaSource.cc
    ../src/Decoder.cc
    ../src/utils/DecoderContext.cc
)

# --- 2. 找到依赖的 FFmpeg 库 ---
# 这是在 Linux 系统上查找库的标准方法
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libavutil)

# --- 3. 定义 player_lib 库 ---
add_library(player_lib STATIC ${PLAYER_LIB_SOURCES})

# 为 player_lib 添加头文件搜索路径和链接库
target_include_directories(player_lib PUBLIC
    # 添加项目头文件目录
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../common/include
    # 添加 FFmpeg 头文件目录
    ${FFMPEG_INCLUDE_DIRS}
)
# 将 player_lib 链接到 FFmpeg
target_link_libraries(player_lib PRIVATE ${FFMPEG_LIBRARIES})

# --- 4. 配置 Google Test ---
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# 对于 CMP0135 警告，我们可以设置 NEW 行为
set(FETCHCONTENT_DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(googletest)

# --- 5. 定义测试可执行文件 ---
# 注意：文件名从 demuxer_test.cpp 改为了 test_demuxer.cc
add_executable(run_demuxer_tests test_demuxer.cc)

# 将测试可执行文件链接到 GTest 和我们自己的 player_lib
target_link_libraries(run_demuxer_tests PRIVATE
    gtest_main
    player_lib
)

add_executable(run_decoder_tests test_decoder.cc)

target_link_libraries(run_decoder_tests PRIVATE
    gtest_main
    player_lib
)


# GTest 需要 pthreads
find_package(Threads REQUIRED)
target_link_libraries(run_demuxer_tests PRIVATE Threads::Threads)
