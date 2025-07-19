add_executable(JpegReaderSDL main_SDL.cc SDLDisplay.cc)

target_include_directories(JpegReaderSDL PRIVATE ${PROJECT_SOURCE_DIR}/include)

target_link_directories(JpegReaderSDL PRIVATE ${JPEG_TURBO_LIB_DIR})

target_link_libraries(JpegReaderSDL PRIVATE
    JpegDecoder
    SDL2
)

add_dependencies(JpegReaderSDL external_libjpeg_turbo)

if(UNIX)
    target_link_options(JpegReaderSDL PRIVATE
        -Wl,-rpath,${JPEG_TURBO_LIB_DIR}
    )
endif()
