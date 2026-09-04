cmake_minimum_required(VERSION 3.16)
project(map_compiler C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(OpenMP REQUIRED)

set(EMBREE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/embree")
set(OIDN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/oidn")

if(WIN32)
    set(EMBREE_LIB_DIR "${EMBREE_DIR}/lib/x64")
    set(OIDN_LIB_DIR "${OIDN_DIR}/lib/x64")
else()
    set(EMBREE_LIB_DIR "${EMBREE_DIR}/lib/linux")
    set(OIDN_LIB_DIR "${OIDN_DIR}/lib/linux")
    set(CMAKE_BUILD_RPATH "${EMBREE_LIB_DIR}")
    set(CMAKE_INSTALL_RPATH "${EMBREE_LIB_DIR}")
endif()

file(GLOB_RECURSE SRC_FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c"
)

file(GLOB_RECURSE HEADER_FILES 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" FILES ${SRC_FILES} ${HEADER_FILES})

include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${EMBREE_DIR}/include"
    "${OIDN_DIR}/include"
)

link_directories("${EMBREE_LIB_DIR}" "${OIDN_LIB_DIR}")

add_executable(compiler ${SRC_FILES} ${HEADER_FILES})

target_compile_definitions(compiler PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(compiler PRIVATE embree4 OpenImageDenoise OpenMP::OpenMP_CXX)

if(MSVC)
    target_compile_options(compiler PRIVATE /W3 /MP /O2)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT compiler)
    
    add_custom_command(TARGET compiler POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${EMBREE_DIR}/bin/embree4.dll"
        $<TARGET_FILE_DIR:compiler>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${EMBREE_DIR}/bin/tbb12.dll"
        $<TARGET_FILE_DIR:compiler>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${OIDN_DIR}/bin/OpenImageDenoise.dll"
        $<TARGET_FILE_DIR:compiler>
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${OIDN_DIR}/bin"
        $<TARGET_FILE_DIR:compiler>
    )
endif()