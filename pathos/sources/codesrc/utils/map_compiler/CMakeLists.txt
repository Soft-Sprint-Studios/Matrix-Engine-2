cmake_minimum_required(VERSION 3.16)
project(map_compiler C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenMP REQUIRED)
find_package(Vulkan REQUIRED)

set(OIDN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/oidn")

if(WIN32)
    set(OIDN_LIB_DIR "${OIDN_DIR}/lib/x64")
else()
    set(OIDN_LIB_DIR "${OIDN_DIR}/lib/linux")
    set(CMAKE_BUILD_RPATH "${OIDN_LIB_DIR}")
    set(CMAKE_INSTALL_RPATH "${OIDN_LIB_DIR}")
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
    "${OIDN_DIR}/include"
    ${Vulkan_INCLUDE_DIRS}
)

link_directories("${OIDN_LIB_DIR}")

add_executable(compiler ${SRC_FILES} ${HEADER_FILES})

target_compile_definitions(compiler PRIVATE _CRT_SECURE_NO_WARNINGS)
target_link_libraries(compiler PRIVATE Vulkan::Vulkan OpenImageDenoise OpenMP::OpenMP_CXX)

find_program(GLSLANG_VALIDATOR NAMES glslangValidator glslc HINTS $ENV{VULKAN_SDK}/bin)

set(SHADER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/src/vis.comp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ray_occlude.comp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ray_intersect.comp"
)

set(SHADER_OUTPUTS "")

foreach(SHADER_SRC ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER_SRC} NAME)
    set(SPV_OUT "${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.spv")

    if(GLSLANG_VALIDATOR MATCHES "glslc")
        add_custom_command(
            OUTPUT ${SPV_OUT}
            COMMAND ${GLSLANG_VALIDATOR} "${SHADER_SRC}" -o "${SPV_OUT}"
            DEPENDS "${SHADER_SRC}"
            COMMENT "Compiling ${SHADER_NAME} to SPIR-V with glslc"
        )
    else()
        add_custom_command(
            OUTPUT ${SPV_OUT}
            COMMAND ${GLSLANG_VALIDATOR} -V "${SHADER_SRC}" -o "${SPV_OUT}"
            DEPENDS "${SHADER_SRC}"
            COMMENT "Compiling ${SHADER_NAME} to SPIR-V with glslangValidator"
        )
    endif()

    list(APPEND SHADER_OUTPUTS ${SPV_OUT})
endforeach()

source_group("Shaders" FILES ${SHADER_SOURCES})
target_sources(compiler PRIVATE ${SHADER_SOURCES} ${SHADER_OUTPUTS})

if(MSVC)
    target_compile_options(compiler PRIVATE /W3 /MP /O2)
    set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT compiler)
    
    add_custom_command(TARGET compiler POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SHADER_OUTPUTS}
        $<TARGET_FILE_DIR:compiler>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${OIDN_DIR}/bin/OpenImageDenoise.dll"
        $<TARGET_FILE_DIR:compiler>
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${OIDN_DIR}/bin"
        $<TARGET_FILE_DIR:compiler>
        COMMENT "Copying runtime DLLs and compiled shaders"
    )
else()
    add_custom_command(TARGET compiler POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${SHADER_OUTPUTS}
        $<TARGET_FILE_DIR:compiler>
        COMMENT "Copying compiled shaders"
    )
endif()