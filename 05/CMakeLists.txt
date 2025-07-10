cmake_minimum_required(VERSION 3.0)
project(SharedLibExample)

add_subdirectory(lib)
add_executable(shared_hello main.cc)
target_link_libraries(shared_hello PRIVATE hello)
