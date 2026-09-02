cmake_minimum_required(VERSION 4.0)
cmake_policy(SET CMP0091 NEW)
project(mdlimporter LANGUAGES CXX)

add_executable(mdlimporter)

target_sources(mdlimporter
    PRIVATE
        mdlimporter.cpp
)

target_include_directories(mdlimporter
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_features(mdlimporter
    PRIVATE
        cxx_std_17
)

target_compile_definitions(mdlimporter
    PRIVATE
        NDEBUG
        _CRT_SECURE_NO_WARNINGS
        $<$<PLATFORM_ID:Windows>:_CONSOLE>
        $<$<NOT:$<PLATFORM_ID:Windows>>:_LINUX>
)

target_compile_options(mdlimporter
        PRIVATE
            "/MP"
            "/W3"
    )

if (MSVC)
    set_target_properties(mdlimporter PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
        MSVC_RUNTIME_CHECKS ""
    )
endif()

source_group("Source Files" FILES mdlimporter.cpp)