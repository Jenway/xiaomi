cmake_minimum_required(VERSION 3.14)
project(AndroidMediaPlayer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_library(common_includes INTERFACE)
target_include_directories(common_includes INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/common/include>
    $<INSTALL_INTERFACE:include>
)


add_subdirectory(ffmpegJNI)
# add_subdirectory(videoFrameRender)

