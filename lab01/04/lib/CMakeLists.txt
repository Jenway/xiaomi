cmake_minimum_required(VERSION 3.0)

add_library(hello STATIC lib.cc)
target_include_directories(hello PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
