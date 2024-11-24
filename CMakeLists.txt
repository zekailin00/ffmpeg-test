cmake_minimum_required(VERSION 3.22)
project(FFmpegDecoder)

set (CMAKE_CXX_STANDARD 17)

add_subdirectory(ffmpeg)
add_subdirectory(${CMAKE_SOURCE_DIR}/imgui)


add_executable(streamDecoder
    main.cpp
    decoder.cpp
    inspector.cpp
)
target_link_libraries(streamDecoder PUBLIC FFMPEG imgui)


add_executable(streamServer
    encoder.cpp
    main_server.cpp
)
target_link_libraries(streamServer PUBLIC FFMPEG)


add_executable(scaleDebug
    main_scale.cpp
)
target_link_libraries(scaleDebug PUBLIC FFMPEG imgui)

