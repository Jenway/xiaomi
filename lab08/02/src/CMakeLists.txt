add_executable(math_app main.cc)

target_link_libraries(math_app PRIVATE math_add math_sub)
