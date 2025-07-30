cmake_minimum_required(VERSION 3.14)

project(ffmpeg_utils_jni)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ==== Debug messages ====
message(STATUS ">>> FFMPEG_INCLUDE_DIR: ${FFMPEG_INCLUDE_DIR}")
message(STATUS ">>> FFMPEG_LIB_DIR: ${FFMPEG_LIB_DIR}")
message(STATUS ">>> ANDROID_NDK: $ENV{ANDROID_NDK}")

# ==== Build Target ====
add_library(ffmpeg_utils_jni SHARED native-lib.cpp)

# ==== Include Directories ====
target_include_directories(ffmpeg_utils_jni PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${FFMPEG_INCLUDE_DIR}
    $ENV{ANDROID_NDK}/sysroot/usr/include
    $ENV{ANDROID_NDK}/sysroot/usr/include/android
)

# ==== Find log library ====
find_library(LOG_LIB log)
if (LOG_LIB)
    message(STATUS ">>> Found log-lib: ${LOG_LIB}")
else()
    message(FATAL_ERROR "!!! log-lib not found")
endif()

# ==== Link Libraries ====
target_link_libraries(ffmpeg_utils_jni
    ${LOG_LIB}
    "${FFMPEG_LIB_DIR}/libavutil.so"
    "${FFMPEG_LIB_DIR}/libavcodec.so"
    "${FFMPEG_LIB_DIR}/libavformat.so"
    "${FFMPEG_LIB_DIR}/libswscale.so"
    "${FFMPEG_LIB_DIR}/libswresample.so"
)
