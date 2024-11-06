cmake_minimum_required(VERSION 3.22)
project(FFmpegDecoder)

set (CMAKE_CXX_STANDARD 17)

add_executable(streamDecoder
    main.cpp
)

file(GLOB FFMPEG_LIBS "${CMAKE_SOURCE_DIR}/ffmpeg/lib/*.a")

add_subdirectory(ffmpeg)

target_link_libraries(streamDecoder PUBLIC FFMPEG)
