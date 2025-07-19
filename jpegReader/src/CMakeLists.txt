# src/CMakeLists.txt

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_library(JpegDecoder STATIC JpegDecoder.cc)

target_include_directories(JpegDecoder PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${JPEG_TURBO_INSTALL}/include
)

target_link_directories(JpegDecoder PUBLIC
    ${JPEG_TURBO_INSTALL}/lib
)

target_link_libraries(JpegDecoder PUBLIC
    jpeg # Link against 'jpeg' library name. With target_link_directories, it will find it.
)

# Ensure JpegDecoder is built AFTER external_libjpeg_turbo
add_dependencies(JpegDecoder external_libjpeg_turbo)

# 添加 UI 模块
add_subdirectory(ascii)
add_subdirectory(opengl)
add_subdirectory(sdl)
