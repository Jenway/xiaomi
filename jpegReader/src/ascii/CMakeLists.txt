add_executable(JpegReaderASCII
    main_ASCII.cc
    AsciiArtDisplay.cc
)

target_include_directories(JpegReaderASCII PRIVATE ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(JpegReaderASCII
    PRIVATE
    JpegDecoder
    caca
)

if(UNIX)
    target_link_options(JpegReaderASCII PRIVATE
        -Wl,-rpath,${JPEG_TURBO_LIB_DIR}
    )
endif()
