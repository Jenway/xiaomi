cmake_minimum_required(VERSION 3.10)
project(fileServer)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(fileServer src/Server.cc src/Socket.cc src/Poller.cc src/Connection.cc)
target_include_directories(fileServer PRIVATE include lib)

add_executable(fileClient src/Client.cc src/ClientConnection.cc)
target_include_directories(fileClient PRIVATE include lib)
