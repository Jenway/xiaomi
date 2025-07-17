cmake_minimum_required(VERSION 3.10)

project(math_utils_jni)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
