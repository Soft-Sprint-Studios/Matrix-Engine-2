cmake_minimum_required(VERSION 4.0)
cmake_policy(SET CMP0091 NEW)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(matrix-engine)

add_subdirectory(clientdll)
add_subdirectory(engine)
add_subdirectory(gamedll)
if (WIN32)
add_subdirectory(utils/mdlimporter)
endif()
add_subdirectory(utils/vbmcompiler)
