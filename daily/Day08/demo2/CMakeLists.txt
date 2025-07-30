cmake_minimum_required(VERSION 3.10)
project(MyProject)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 生成动态库
add_library(my_shared_lib SHARED dy/add.cc)
target_include_directories(my_shared_lib PUBLIC dy/include)

# 生成静态库
add_library(my_static_lib STATIC st/sub.cc)
target_include_directories(my_static_lib PUBLIC st/include)
# 生成可执行文件
add_executable(my_app main.cc)
target_link_libraries(my_app PRIVATE my_shared_lib)
target_link_libraries(my_app PRIVATE my_static_lib)

# 可选：设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin) # 可执行文件输出到 bin/
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib) # 动态库输出到 lib/
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib) # 静态库输出到 lib/
