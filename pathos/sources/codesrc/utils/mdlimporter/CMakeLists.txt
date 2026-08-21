cmake_minimum_required(VERSION 4.0)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(mdlimporter LANGUAGES CXX)

add_executable(mdlimporter)

target_sources(mdlimporter
    PRIVATE
        mdlimporter.cpp
)

set_target_properties(mdlimporter PROPERTIES
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    MSVC_RUNTIME_CHECKS ""
)

target_compile_features(mdlimporter
    PRIVATE
        cxx_std_17
)

target_compile_definitions(mdlimporter
    PRIVATE
        NDEBUG
        _CONSOLE
        UNICODE
        _UNICODE
)

target_compile_options(mdlimporter
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/MP>"
        "$<$<CXX_COMPILER_ID:MSVC>:/W3>"
)


source_group("Source Files" FILES mdlimporter.cpp)
