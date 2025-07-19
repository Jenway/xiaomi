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
    jpeg
)

add_dependencies(JpegDecoder external_libjpeg_turbo)

add_subdirectory(opengl)
add_subdirectory(sdl)
