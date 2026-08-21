cmake_minimum_required(VERSION 4.0)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(vbmcompiler LANGUAGES CXX)

add_executable(vbmcompiler)

target_sources(vbmcompiler
    PRIVATE
        ../../common/bmp.cpp
        ../../common/bmp.h
        ../../common/bmpformat.h
        ../../common/carray_inline.hpp
        ../../common/carray.h
        ../../common/cbitset_inline.hpp
        ../../common/cbitset.cpp
        ../../common/cbitset.h
        ../../common/cbuffer_inline.hpp
        ../../common/cbuffer.cpp
        ../../common/cbuffer.h
        ../../common/clinkedlist_inline.hpp
        ../../common/clinkedlist.h
        ../../common/common_inline.hpp
        ../../common/common.cpp
        ../../common/common.h
        ../../common/constants.h
        ../../common/crc32.cpp
        ../../common/crc32.h
        ../../common/cstring_inline.hpp
        ../../common/cstring.cpp
        ../../common/cstring.h
        ../../common/cstringpool_inline.hpp
        ../../common/cstringpool.cpp
        ../../common/cstringpool.h
        ../../common/logfile.cpp
        ../../common/logfile.h
        ../../common/matrix.cpp
        ../../common/matrix.h
        ../../common/r_common.cpp
        ../../common/r_common.h
        ../../common/tga.cpp
        ../../common/tga.h
        ../../common/tgaformat.h
        ../../common/vector_inline.hpp
        ../../common/vector.h
        ../../common/winding.h
        ../../shared/mcdformat.h
        ../../shared/studio.h
        ../../shared/vbm_shared.cpp
        ../../shared/vbm_shared.h
        ../../shared/vbmformat.h
        ../common/utils_common.cpp
        ../common/utils_common.h
        ../common/utils_filefuncs.cpp
        ../common/utils_filefuncs.h
        animationsmdparser.cpp
        animationsmdparser.h
        collisionsmdparser.cpp
        collisionsmdparser.h
        compiler_math.cpp
        compiler_math.h
        compiler_types.cpp
        compiler_types.h
        geometrysmdparser.cpp
        geometrysmdparser.h
        main.cpp
        main.h
        mcdcompiler.cpp
        mcdcompiler.h
        options.cpp
        options.h
        parserbase.cpp
        parserbase.h
        qcparser.cpp
        qcparser.h
        refframesmdparser.cpp
        refframesmdparser.h
        smdparser.cpp
        smdparser.h
        studiocompiler.cpp
        studiocompiler.h
        trimeshbuilder.cpp
        trimeshbuilder.h
        vbmcompiler.cpp
        vbmcompiler.h
        vtaparser.cpp
        vtaparser.h
)

set_target_properties(vbmcompiler PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/Debug"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Debug"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/Debug"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Debug"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/Debug"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    MSVC_RUNTIME_CHECKS ""
)

target_include_directories(vbmcompiler
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/glad/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../common"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../shared"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/SDL3-3.4.14/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../common"
)

target_compile_definitions(vbmcompiler
    PRIVATE
        WIN32
        NDEBUG
        _CONSOLE
        vbmcompiler
)

target_link_directories(vbmcompiler
    PRIVATE
        ../../x64/Release
)

target_link_libraries(vbmcompiler
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../libs/SDL3-3.4.14/lib/x64/SDL3.lib"
)

target_compile_options(vbmcompiler
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/MP>"
        "$<$<CXX_COMPILER_ID:MSVC>:/wd4996>"
        "$<$<CXX_COMPILER_ID:MSVC>:/wd4244>"
        "$<$<CXX_COMPILER_ID:MSVC>:/wd4237>"
        "$<$<CXX_COMPILER_ID:MSVC>:/wd4305>"
        "$<$<CXX_COMPILER_ID:MSVC>:/W3>"
)


source_group("Source Files/common" FILES ../../common/bmp.cpp ../../common/cbitset.cpp ../../common/cbuffer.cpp ../../common/common.cpp ../../common/crc32.cpp ../../common/cstring.cpp ../../common/cstringpool.cpp ../../common/logfile.cpp ../../common/matrix.cpp ../../common/r_common.cpp ../../common/tga.cpp ../common/utils_common.cpp ../common/utils_filefuncs.cpp)
source_group("Source Files/shared" FILES ../../shared/vbm_shared.cpp)
source_group("Source Files" FILES collisionsmdparser.cpp compiler_math.cpp compiler_types.cpp main.cpp mcdcompiler.cpp options.cpp parserbase.cpp qcparser.cpp geometrysmdparser.cpp animationsmdparser.cpp refframesmdparser.cpp smdparser.cpp studiocompiler.cpp trimeshbuilder.cpp vbmcompiler.cpp vtaparser.cpp)
source_group("Header Files/common" FILES ../../common/bmp.h ../../common/bmpformat.h ../../common/carray.h ../../common/carray_inline.hpp ../../common/cbitset.h ../../common/cbitset_inline.hpp ../../common/cbuffer.h ../../common/cbuffer_inline.hpp ../../common/clinkedlist.h ../../common/clinkedlist_inline.hpp ../../common/common.h ../../common/common_inline.hpp ../../common/constants.h ../../common/crc32.h ../../common/cstring.h ../../common/cstringpool.h ../../common/cstringpool_inline.hpp ../../common/cstring_inline.hpp ../../common/logfile.h ../../common/matrix.h ../../common/r_common.h ../../common/tga.h ../../common/tgaformat.h ../../common/vector.h ../../common/vector_inline.hpp ../../common/winding.h ../common/utils_common.h ../common/utils_filefuncs.h)
source_group("Header Files/shared" FILES ../../shared/mcdformat.h ../../shared/studio.h ../../shared/vbmformat.h ../../shared/vbm_shared.h)
source_group("Header Files" FILES collisionsmdparser.h compiler_math.h compiler_types.h main.h mcdcompiler.h options.h parserbase.h qcparser.h geometrysmdparser.h animationsmdparser.h refframesmdparser.h smdparser.h studiocompiler.h trimeshbuilder.h vbmcompiler.h vtaparser.h)
