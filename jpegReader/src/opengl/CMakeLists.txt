find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(JpegReaderOpenGL main_OpenGL.cc OpenGLDisplay.cc)

target_include_directories(JpegReaderOpenGL PRIVATE ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(JpegReaderOpenGL PRIVATE
    JpegDecoder
    OpenGL::GL
    glfw
)

if(UNIX)
    target_link_options(JpegReaderOpenGL PRIVATE
        -Wl,-rpath,${JPEG_TURBO_LIB_DIR}
    )
endif()
