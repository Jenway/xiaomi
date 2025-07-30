# 例子 字符串移除  
cmake_minimum_required(VERSION 3.0)
project(TEST)

set(TEMP "hello,world")
file(GLOB SRC_1 ${PROJECT_SOURCE_DIR}/*.cpp)
message(STATUS "message: ${SRC_1}")

list(APPEND SRC_1 ${PROJECT_SOURCE_DIR}/main.cpp)
set(find_out "")

list(FIND SRC_1 "hello world" find_out)

# 移除后日志  
message(STATUS "message: ${SRC_1}")
message(STATUS "message: ${find_out}")
