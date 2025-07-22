option(BUILD_ANDROID_LIB "Build Android JNI shared library" ON)

add_subdirectory(src)

if(NOT BUILD_ANDROID_LIB)
    add_subdirectory(test)
endif()
