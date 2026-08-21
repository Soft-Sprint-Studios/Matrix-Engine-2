cmake_minimum_required(VERSION 4.0)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(libogg_static LANGUAGES C)

add_library(libogg_static STATIC)

target_sources(libogg_static
    PRIVATE
        ../../include/ogg/ogg.h
        ../../include/ogg/os_types.h
        ../../src/bitwise.c
        ../../src/framing.c
)

set_target_properties(libogg_static PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/$(Platform)/$<CONFIG>"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/$(Platform)/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/$(Platform)/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/$(Platform)/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/$(Platform)/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/$(Platform)/$<CONFIG>"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    MSVC_RUNTIME_CHECKS ""
)

target_include_directories(libogg_static
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../include"
)

target_compile_definitions(libogg_static
    PRIVATE
        WIN32
        NDEBUG
        _WINDOWS
        _USRDLL
        LIBOGG_EXPORTS
        UNICODE
        _UNICODE
)

target_compile_options(libogg_static
    PRIVATE
        "$<$<C_COMPILER_ID:MSVC>:/MP>"
        "$<$<C_COMPILER_ID:MSVC>:/wd4244>"
        "$<$<C_COMPILER_ID:MSVC>:/W4>"
)


source_group("Source Files/src" FILES ../../src/bitwise.c ../../src/framing.c)
source_group("Header Files/include/ogg" FILES ../../include/ogg/ogg.h ../../include/ogg/os_types.h)
