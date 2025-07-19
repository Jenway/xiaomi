cmake_minimum_required(VERSION 3.14)
project(JpegReader LANGUAGES CXX)

include(ExternalProject)
include(ProcessorCount)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

ProcessorCount(NPROC)
if(NOT NPROC EQUAL 0)
        set(BUILD_PARALLEL_JOBS -j${NPROC})
endif()

set(EXTERNAL_INSTALL_DIR ${CMAKE_BINARY_DIR}/external_deps)
file(MAKE_DIRECTORY ${EXTERNAL_INSTALL_DIR})

ExternalProject_Add(
        external_libjpeg_turbo
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/lib/libjpeg-turbo
        INSTALL_DIR ${EXTERNAL_INSTALL_DIR}/libjpeg-turbo
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DENABLE_SHARED=TRUE -DENABLE_STATIC=FALSE
        LOG_CONFIGURE TRUE LOG_BUILD TRUE LOG_INSTALL TRUE
)

ExternalProject_Get_Property(external_libjpeg_turbo INSTALL_DIR)
set(JPEG_TURBO_INSTALL ${INSTALL_DIR})


# External: notcurses
ExternalProject_Add(
        external_notcurses
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/lib/notcurses
        INSTALL_DIR ${EXTERNAL_INSTALL_DIR}/notcurses
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DUSE_STATIC=OFF -DUSE_PANDOC=OFF -DUSE_DOXYGEN=OFF
        LOG_CONFIGURE TRUE LOG_BUILD TRUE LOG_INSTALL TRUE
)

ExternalProject_Get_Property(external_notcurses INSTALL_DIR)
set(NOTCURSES_INSTALL ${INSTALL_DIR})

# Expose to subdirs
set(JPEG_TURBO_INCLUDE_DIR ${JPEG_TURBO_INSTALL}/include)
set(JPEG_TURBO_LIB_DIR ${JPEG_TURBO_INSTALL}/lib)

set(NOTCURSES_INCLUDE_DIR ${NOTCURSES_INSTALL}/include)
set(NOTCURSES_LIB_DIR ${NOTCURSES_INSTALL}/lib)

add_subdirectory(src)
