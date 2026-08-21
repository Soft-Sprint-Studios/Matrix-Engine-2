cmake_minimum_required(VERSION 4.0)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(libvorbis_static LANGUAGES C)

add_library(libvorbis_static STATIC)

target_sources(libvorbis_static
    PRIVATE
        ../../../include/vorbis/codec.h
        ../../../include/vorbis/vorbisenc.h
        ../../../include/vorbis/vorbisfile.h
        ../../../lib/analysis.c
        ../../../lib/backends.h
        ../../../lib/bitrate.c
        ../../../lib/bitrate.h
        ../../../lib/block.c
        ../../../lib/books/coupled/res_books_stereo.h
        ../../../lib/books/floor/floor_books.h
        ../../../lib/books/uncoupled/res_books_uncoupled.h
        ../../../lib/codebook.c
        ../../../lib/codebook.h
        ../../../lib/codec_internal.h
        ../../../lib/envelope.c
        ../../../lib/envelope.h
        ../../../lib/floor0.c
        ../../../lib/floor1.c
        ../../../lib/highlevel.h
        ../../../lib/info.c
        ../../../lib/lookup_data.h
        ../../../lib/lookup.c
        ../../../lib/lookup.h
        ../../../lib/lpc.c
        ../../../lib/lpc.h
        ../../../lib/lsp.c
        ../../../lib/lsp.h
        ../../../lib/mapping0.c
        ../../../lib/masking.h
        ../../../lib/mdct.c
        ../../../lib/mdct.h
        ../../../lib/misc.h
        ../../../lib/modes/floor_all.h
        ../../../lib/modes/psych_11.h
        ../../../lib/modes/psych_16.h
        ../../../lib/modes/psych_44.h
        ../../../lib/modes/psych_8.h
        ../../../lib/modes/residue_16.h
        ../../../lib/modes/residue_44.h
        ../../../lib/modes/residue_44u.h
        ../../../lib/modes/residue_8.h
        ../../../lib/modes/setup_11.h
        ../../../lib/modes/setup_16.h
        ../../../lib/modes/setup_22.h
        ../../../lib/modes/setup_32.h
        ../../../lib/modes/setup_44.h
        ../../../lib/modes/setup_44u.h
        ../../../lib/modes/setup_8.h
        ../../../lib/modes/setup_X.h
        ../../../lib/os.h
        ../../../lib/psy.c
        ../../../lib/psy.h
        ../../../lib/registry.c
        ../../../lib/registry.h
        ../../../lib/res0.c
        ../../../lib/scales.h
        ../../../lib/sharedbook.c
        ../../../lib/smallft.c
        ../../../lib/smallft.h
        ../../../lib/synthesis.c
        ../../../lib/vorbisenc.c
        ../../../lib/window.c
        ../../../lib/window.h
)

set_target_properties(libvorbis_static PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/$(Platform)/$<CONFIG>"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/$(Platform)/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/$(Platform)/$<CONFIG>"
    LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/$(Platform)/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/$(Platform)/$<CONFIG>"
    ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/$(Platform)/$<CONFIG>"
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    MSVC_RUNTIME_CHECKS ""
)

target_include_directories(libvorbis_static
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../../libogg/include"
)

target_compile_definitions(libvorbis_static
    PRIVATE
        WIN32
        NDEBUG
        _WINDOWS
        _USRDLL
        LIBVORBIS_EXPORTS
        UNICODE
        _UNICODE
)

target_compile_options(libvorbis_static
    PRIVATE
        "$<$<C_COMPILER_ID:MSVC>:/MP>"
        "$<$<C_COMPILER_ID:MSVC>:/wd4244>"
        "$<$<C_COMPILER_ID:MSVC>:/wd4100>"
        "$<$<C_COMPILER_ID:MSVC>:/wd4267>"
        "$<$<C_COMPILER_ID:MSVC>:/wd4189>"
        "$<$<C_COMPILER_ID:MSVC>:/wd4305>"
        "$<$<C_COMPILER_ID:MSVC>:/wd4127>"
        "$<$<C_COMPILER_ID:MSVC>:/wd4706>"
        "$<$<C_COMPILER_ID:MSVC>:/W4>"
)


source_group("Source Files/lib" FILES ../../../lib/analysis.c ../../../lib/bitrate.c ../../../lib/block.c ../../../lib/codebook.c ../../../lib/envelope.c ../../../lib/floor0.c ../../../lib/floor1.c ../../../lib/info.c ../../../lib/lookup.c ../../../lib/lpc.c ../../../lib/lsp.c ../../../lib/mapping0.c ../../../lib/mdct.c ../../../lib/psy.c ../../../lib/registry.c ../../../lib/res0.c ../../../lib/sharedbook.c ../../../lib/smallft.c ../../../lib/synthesis.c ../../../lib/vorbisenc.c ../../../lib/window.c)
source_group("Header Files/lib" FILES ../../../lib/backends.h ../../../lib/bitrate.h ../../../lib/codebook.h ../../../lib/codec_internal.h ../../../lib/envelope.h ../../../lib/highlevel.h ../../../lib/lookup.h ../../../lib/lookup_data.h ../../../lib/lpc.h ../../../lib/lsp.h ../../../lib/masking.h ../../../lib/mdct.h ../../../lib/misc.h ../../../lib/os.h ../../../lib/psy.h ../../../lib/registry.h ../../../lib/scales.h ../../../lib/smallft.h ../../../lib/window.h)
source_group("Header Files/include/vorbis" FILES ../../../include/vorbis/codec.h ../../../include/vorbis/vorbisenc.h ../../../include/vorbis/vorbisfile.h)
source_group("Header Files/lib/modes" FILES ../../../lib/modes/floor_all.h ../../../lib/modes/psych_11.h ../../../lib/modes/psych_16.h ../../../lib/modes/psych_44.h ../../../lib/modes/psych_8.h ../../../lib/modes/residue_16.h ../../../lib/modes/residue_44.h ../../../lib/modes/residue_44u.h ../../../lib/modes/residue_8.h ../../../lib/modes/setup_11.h ../../../lib/modes/setup_16.h ../../../lib/modes/setup_22.h ../../../lib/modes/setup_32.h ../../../lib/modes/setup_44.h ../../../lib/modes/setup_44u.h ../../../lib/modes/setup_8.h ../../../lib/modes/setup_X.h)
source_group("Header Files/lib/books/floor" FILES ../../../lib/books/floor/floor_books.h)
source_group("Header Files/lib/books/coupled" FILES ../../../lib/books/coupled/res_books_stereo.h)
source_group("Header Files/lib/books/uncoupled" FILES ../../../lib/books/uncoupled/res_books_uncoupled.h)
