message(STATUS "Building Android JNI .so with cross-compiled FFmpeg")

if(NOT FFMPEG_ANDROID_INCLUDE_DIR OR NOT FFMPEG_ANDROID_LIB_DIR)
    message(FATAL_ERROR "Please set FFMPEG_ANDROID_INCLUDE_DIR and FFMPEG_ANDROID_LIB_DIR")
endif()

file(GLOB UITLS_SOURCES "utils/*.cc")
add_library(mp4parser_core STATIC
    Demuxer.cc
    Decoder.cc
    ${UITLS_SOURCES}
)

target_include_directories(mp4parser_core
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<BUILD_INTERFACE:${FFMPEG_ANDROID_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(mp4parser_core PUBLIC common_includes)
target_compile_features(mp4parser_core PUBLIC cxx_std_17)

add_library(mp4parser STATIC Mp4Parser.cc)

find_package(JNI REQUIRED)

target_include_directories(mp4parser
    PRIVATE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    ${JNI_INCLUDE_DIRS}
    ${FFMPEG_ANDROID_INCLUDE_DIR}
)

target_link_directories(mp4parser
    PUBLIC
    ${FFMPEG_ANDROID_LIB_DIR}
)

target_link_libraries(mp4parser
    PUBLIC
    mp4parser_core
    avcodec
    avformat
    avutil
    swresample
    swscale
    z
    android
    log
    ${JNI_LIBRARIES}
)


target_compile_features(mp4parser PUBLIC cxx_std_17)

# No need to install this
# install(TARGETS mp4parser_core
#     ARCHIVE DESTINATION "${INSTALL_LIB_DIR}"
# )

# install(TARGETS mp4parser
#     ARCHIVE DESTINATION "${INSTALL_LIB_DIR}"
# )
