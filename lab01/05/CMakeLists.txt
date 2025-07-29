cmake_minimum_required(VERSION 3.0)
project(SharedLibExample)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

add_subdirectory(lib)
add_executable(shared_hello main.cc)
target_link_libraries(shared_hello PRIVATE hello)

set_target_properties(shared_hello PROPERTIES
    BUILD_RPATH "$ORIGIN"
    INSTALL_RPATH "$ORIGIN"
)
