
list(APPEND FFMPEG_LIB)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # message("-- [FFMPEG] system is Windows")
    # set(os_name "windows")
    # set(lib_suffix "lib")
    # set(lib_type "static")

    # list(APPEND FFMPEG_LIB
    #     FFMPEG_libbase
    #     FFMPEG_libplatform
    #     FFMPEG_monolith
    # )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    # message("-- [FFMPEG] system is MacOS")
    # set(os_name "mac")
    # set(lib_suffix "dylib")
    # set(lib_type "shared")

    # list(APPEND FFMPEG_LIB
    #     libFFMPEG 
    #     libFFMPEG_libplatform
    #     libFFMPEG_libbase
    #     libthird_party_icu_icui18n
    #     libicuuc
    #     libchrome_zlib
    # )
else()
    # message(FATAL_ERROR "-- [FFMPEG] FFMPEG can only be compiled on Windows or MacOS" )
    message("-- [FFMPEG] system is Ubuntu")
    set(lib_suffix "a")
    set(lib_type "static")

    list(APPEND FFMPEG_LIB
        libavdevice
        libavformat
        libavfilter
        libavcodec
        libswresample
        libswscale
        libavutil
    )

    # libavdevice                        \
    # libavformat                        \
    # libavfilter                        \
    # libavcodec                         \
    # libswresample                      \
    # libswscale                         \
    # libavutil                          \

endif()

set (build_opt "debug")

foreach(lib IN LISTS FFMPEG_LIB)
    if(lib_type STREQUAL "static")
        add_library(${lib} STATIC IMPORTED)
    else()
        add_library(${lib} SHARED IMPORTED)
    endif()

    set(import_path ${CMAKE_CURRENT_SOURCE_DIR}/lib/${lib}.${lib_suffix})
    message("-- [FFMPEG] Importing ${lib_type} library ${import_path}")
    set_property(
        TARGET ${lib}
        PROPERTY IMPORTED_LOCATION ${import_path}
    )
endforeach()

add_library(FFMPEG INTERFACE)
target_include_directories(FFMPEG INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(FFMPEG INTERFACE ${FFMPEG_LIB})

# ZLIB
find_package(ZLIB REQUIRED)
target_include_directories(FFMPEG INTERFACE ${ZLIB_INCLUDE_DIRS})
target_link_libraries(FFMPEG INTERFACE ${ZLIB_LIBRARIES})

# BZip2
find_package(BZip2 REQUIRED)
target_include_directories(FFMPEG INTERFACE ${BZIP2_INCLUDE_DIRS})
target_link_libraries(FFMPEG INTERFACE BZip2::BZip2)

# LZMA
find_package(LibLZMA REQUIRED)
if(LIBLZMA_FOUND)
    message("LZMA found")
    target_include_directories(FFMPEG INTERFACE ${LIBLZMA_INCLUDE_DIRS})
    target_link_libraries(FFMPEG INTERFACE ${LIBLZMA_LIBRARIES})
else()
    message(FATAL_ERROR "LZMA not found")
endif()

target_link_libraries(FFMPEG INTERFACE drm)

message("-- [FFMPEG] All FFMPEG libraries: " "${FFMPEG_LIB}")