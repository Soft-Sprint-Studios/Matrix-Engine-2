cmake_minimum_required(VERSION 4.0)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(freetype LANGUAGES C)

add_library(freetype STATIC)

target_sources(freetype
    PRIVATE
        ../../../include/config/ftconfig.h
        ../../../include/config/ftheader.h
        ../../../include/config/ftmodule.h
        ../../../include/config/ftoption.h
        ../../../include/config/ftstdlib.h
        ../../../include/ft2build.h
        ../../../src/autofit/autofit.c
        ../../../src/base/ftbase.c
        ../../../src/base/ftbbox.c
        ../../../src/base/ftbitmap.c
        ../../../src/base/ftdebug.c
        ../../../src/base/ftfstype.c
        ../../../src/base/ftgasp.c
        ../../../src/base/ftglyph.c
        ../../../src/base/ftgxval.c
        ../../../src/base/ftinit.c
        ../../../src/base/ftlcdfil.c
        ../../../src/base/ftmm.c
        ../../../src/base/ftotval.c
        ../../../src/base/ftpatent.c
        ../../../src/base/ftpfr.c
        ../../../src/base/ftstroke.c
        ../../../src/base/ftsynth.c
        ../../../src/base/ftsystem.c
        ../../../src/base/fttype1.c
        ../../../src/base/ftwinfnt.c
        ../../../src/base/ftxf86.c
        ../../../src/bdf/bdf.c
        ../../../src/cache/ftcache.c
        ../../../src/cff/cff.c
        ../../../src/cid/type1cid.c
        ../../../src/gzip/ftgzip.c
        ../../../src/lzw/ftlzw.c
        ../../../src/pcf/pcf.c
        ../../../src/pfr/pfr.c
        ../../../src/psaux/psaux.c
        ../../../src/pshinter/pshinter.c
        ../../../src/psnames/psmodule.c
        ../../../src/raster/raster.c
        ../../../src/sfnt/sfnt.c
        ../../../src/smooth/smooth.c
        ../../../src/truetype/truetype.c
        ../../../src/type1/type1.c
        ../../../src/type42/type42.c
        ../../../src/winfonts/winfnt.c
)

set_target_properties(freetype PROPERTIES
    OUTPUT_NAME freetype253ST
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../../objs/win32/vc2010"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../../objs/win32/vc2010"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../../objs/win32/vc2010"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../../objs/win32/vc2010"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../../objs/win32/vc2010"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_SOURCE_DIR}/../../../objs/win32/vc2010"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    MSVC_RUNTIME_CHECKS ""
)

target_include_directories(freetype
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../include"
)

target_compile_definitions(freetype
    PRIVATE
        NDEBUG
        WIN32
        _LIB
        _CRT_SECURE_NO_WARNINGS
        FT2_BUILD_LIBRARY
        _MBCS
)

target_compile_options(freetype
    PRIVATE
        "$<$<C_COMPILER_ID:MSVC>:/MP>"
        "$<$<C_COMPILER_ID:MSVC>:/wd4001>"
        "$<$<C_COMPILER_ID:MSVC>:/W4>"
)


source_group("Source Files/src/autofit" FILES ../../../src/autofit/autofit.c)
source_group("Source Files/src/base" FILES ../../../src/base/ftdebug.c ../../../src/base/ftbase.c ../../../src/base/ftbitmap.c ../../../src/base/ftfstype.c ../../../src/base/ftgasp.c ../../../src/base/ftglyph.c ../../../src/base/ftinit.c ../../../src/base/ftstroke.c ../../../src/base/ftsystem.c ../../../src/base/ftbbox.c ../../../src/base/ftmm.c ../../../src/base/ftpfr.c ../../../src/base/ftsynth.c ../../../src/base/fttype1.c ../../../src/base/ftwinfnt.c ../../../src/base/ftxf86.c ../../../src/base/ftlcdfil.c ../../../src/base/ftgxval.c ../../../src/base/ftotval.c ../../../src/base/ftpatent.c)
source_group("Source Files/src/bdf" FILES ../../../src/bdf/bdf.c)
source_group("Source Files/src/cff" FILES ../../../src/cff/cff.c)
source_group("Source Files/src/cache" FILES ../../../src/cache/ftcache.c)
source_group("Source Files/src/gzip" FILES ../../../src/gzip/ftgzip.c)
source_group("Source Files/src/lzw" FILES ../../../src/lzw/ftlzw.c)
source_group("Source Files/src/smooth" FILES ../../../src/smooth/smooth.c)
source_group("Source Files/src/pcf" FILES ../../../src/pcf/pcf.c)
source_group("Source Files/src/pfr" FILES ../../../src/pfr/pfr.c)
source_group("Source Files/src/psaux" FILES ../../../src/psaux/psaux.c)
source_group("Source Files/src/pshinter" FILES ../../../src/pshinter/pshinter.c)
source_group("Source Files/src/psnames" FILES ../../../src/psnames/psmodule.c)
source_group("Source Files/src/raster" FILES ../../../src/raster/raster.c)
source_group("Source Files/src/sfnt" FILES ../../../src/sfnt/sfnt.c)
source_group("Source Files/src/truetype" FILES ../../../src/truetype/truetype.c)
source_group("Source Files/src/type1" FILES ../../../src/type1/type1.c)
source_group("Source Files/src/cid" FILES ../../../src/cid/type1cid.c)
source_group("Source Files/src/type42" FILES ../../../src/type42/type42.c)
source_group("Source Files/src/winfonts" FILES ../../../src/winfonts/winfnt.c)
source_group("Header Files/include" FILES ../../../include/ft2build.h)
source_group("Header Files/include/config" FILES ../../../include/config/ftconfig.h ../../../include/config/ftheader.h ../../../include/config/ftmodule.h ../../../include/config/ftoption.h ../../../include/config/ftstdlib.h)
