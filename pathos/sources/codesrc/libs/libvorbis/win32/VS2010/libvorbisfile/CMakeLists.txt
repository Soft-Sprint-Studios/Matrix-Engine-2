cmake_minimum_required(VERSION 4.0)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(libvorbisfile_static LANGUAGES C)

add_library(libvorbisfile_static STATIC)

target_sources(libvorbisfile_static
    PRIVATE
        ../../../include/vorbis/vorbisfile.h
        ../../../lib/vorbisfile.c
)

set_target_properties(libvorbisfile_static PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/$(Platform)/$<CONFIG>"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/$(Platform)/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/$(Platform)/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/$(Platform)/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/$(Platform)/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/$(Platform)/$<CONFIG>"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    MSVC_RUNTIME_CHECKS ""
)

target_include_directories(libvorbisfile_static
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../../libogg/include"
)

target_compile_definitions(libvorbisfile_static
    PRIVATE
        WIN32
        NDEBUG
        _WINDOWS
        _USRDLL
        LIBVORBISFILE_EXPORTS
        UNICODE
        _UNICODE
)

target_link_libraries(libvorbisfile_static
    PRIVATE
        libvorbis_static
)

target_compile_options(libvorbisfile_static
    PRIVATE
        "$<$<C_COMPILER_ID:MSVC>:/MP>"
        "$<$<C_COMPILER_ID:MSVC>:/W3>"
)


source_group("Source Files/lib" FILES ../../../lib/vorbisfile.c)
source_group("Header Files/include/vorbis" FILES ../../../include/vorbis/vorbisfile.h)
