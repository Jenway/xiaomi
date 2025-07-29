add_library(math_add SHARED src/add.cc)
target_include_directories(math_add PUBLIC ${CMAKE_SOURCE_DIR}/include)
