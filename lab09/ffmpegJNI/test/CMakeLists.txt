find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale libswresample)

# 提取公共设置
set(TEST_SOURCES
    test_main.cc
)

foreach(src ${TEST_SOURCES})
    get_filename_component(test_name ${src} NAME_WE)
    add_executable(${test_name} ${src})

    target_include_directories(${test_name}
        PRIVATE
        ${FFMPEG_INCLUDE_DIRS}
    )

    target_link_directories(${test_name}
        PRIVATE
        ${FFMPEG_LIBRARY_DIRS}
    )

    target_link_libraries(${test_name}
        PRIVATE
        mp4parser_core
        ${FFMPEG_LIBRARIES}
    )

    target_compile_features(${test_name} PRIVATE cxx_std_17)
endforeach()
