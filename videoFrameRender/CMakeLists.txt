cmake_minimum_required(VERSION 3.14)
project(VideoFrameRender)

add_library(video_frame_render STATIC EGLCore.cc src/GLESRender.cc)

target_include_directories(video_frame_render PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(video_frame_render PUBLIC common_includes)

target_link_libraries(video_frame_render PRIVATE
    GLESv2 EGL log android
)
