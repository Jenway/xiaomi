option(BUILD_ANDROID_LIB "Build Android JNI shared library" ON)

if(BUILD_ANDROID_LIB)
    message(STATUS "Building Android JNI .so with cross-compiled FFmpeg")

    if(NOT FFMPEG_ANDROID_INCLUDE_DIR OR NOT FFMPEG_ANDROID_LIB_DIR)
        message(FATAL_ERROR "Please set FFMPEG_ANDROID_INCLUDE_DIR and FFMPEG_ANDROID_LIB_DIR")
    endif()

    add_library(mp4parser_core STATIC mp4_parser.cpp)

    target_include_directories(mp4parser_core
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${FFMPEG_ANDROID_INCLUDE_DIR}>
        # 如果有安装规则，可以用 INSTALL_INTERFACE
    )

    target_compile_features(mp4parser_core PUBLIC cxx_std_17)

    add_library(mp4parser SHARED mp4_parser_jni.cpp jni_helpers.cpp jni_on_load_unload.cpp)

    find_package(JNI REQUIRED)

    target_include_directories(mp4parser
        PRIVATE
        ${JNI_INCLUDE_DIRS}
        ${FFMPEG_ANDROID_INCLUDE_DIR}
    )

    target_link_directories(mp4parser
        PRIVATE
        ${FFMPEG_ANDROID_LIB_DIR}
    )

    target_link_libraries(mp4parser
        PRIVATE
        mp4parser_core
        avcodec avformat avutil swscale swresample
        log
        ${JNI_LIBRARIES}
    )

    target_compile_features(mp4parser PUBLIC cxx_std_17)

else()
    message(STATUS "Building for native test environment")

    add_library(mp4parser_core STATIC mp4_parser.cpp)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale libswresample)

    target_include_directories(mp4parser_core
        PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${FFMPEG_INCLUDE_DIRS}>
    )

    target_link_libraries(mp4parser_core
        PRIVATE
        ${FFMPEG_LIBRARIES}
    )

    target_compile_features(mp4parser_core PUBLIC cxx_std_17)
endif()
