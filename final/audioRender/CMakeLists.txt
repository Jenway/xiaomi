cmake_minimum_required(VERSION 3.22.1)

find_library(
    log-lib
    log
)

add_library(audioRender STATIC
    src/AAudioRender.cpp
    src/ANWRender.cpp
)

target_include_directories(audioRender PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/common/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(audioRender
    android
    log
    aaudio
    ${log-lib})
