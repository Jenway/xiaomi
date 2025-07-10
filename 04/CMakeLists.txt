cmake_minimum_required(VERSION 3.0)
project(StaticLibExample)

add_subdirectory(lib)
add_executable(static_hello main.cc)
target_link_libraries(static_hello PRIVATE hello)
