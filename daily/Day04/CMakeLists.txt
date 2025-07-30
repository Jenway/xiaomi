cmake_minimum_required(VERSION 3.10)
project(min_num)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# add compiler commands json -> /build
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

file(GLOB_RECURSE SOURCES "src/*.cc")

add_executable(jj ${SOURCES})
target_compile_options(jj PRIVATE -Wall -Wextra -Wpedantic -Werror)
target_include_directories(jj PRIVATE include)
