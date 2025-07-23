cmake_minimum_required(VERSION 3.14)
project(AndroidMediaPlayer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(INSTALL_LIB_DIR "${CMAKE_INSTALL_PREFIX}/lib/${ANDROID_ABI}")
set(INSTALL_INCLUDE_DIR "${CMAKE_INSTALL_PREFIX}/include")

install(FILES
    "${PROJECT_SOURCE_DIR}/common/include/Entitys.hpp"
    "${PROJECT_SOURCE_DIR}/common/include/GLRenderHost.hpp"
    "${PROJECT_SOURCE_DIR}/common/include/Mp4Parser.hpp"
    DESTINATION "${INSTALL_INCLUDE_DIR}"
)


add_library(common_includes INTERFACE)
target_include_directories(common_includes INTERFACE
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/common/include>
    $<INSTALL_INTERFACE:include>
)

add_subdirectory(ffmpegJNI)
add_subdirectory(videoFrameRender)

add_library(ffmpeg-jw SHARED common/src/dummy.cc)

set_target_properties(ffmpeg-jw PROPERTIES
    OUTPUT_NAME "ffmpeg-jw"
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
)

target_link_libraries(ffmpeg-jw
    PRIVATE
    "-Wl,--whole-archive"
    mp4parser
    video_frame_render
    "-Wl,--no-whole-archive"
)

install(TARGETS ffmpeg-jw
    LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
)
