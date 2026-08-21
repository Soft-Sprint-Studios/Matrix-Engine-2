cmake_minimum_required(VERSION 4.0)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(client LANGUAGES C CXX)

add_library(client SHARED)

target_sources(client
    PRIVATE
        ../common/buttonbits.h
        ../common/cache_model.h
        ../common/carray_inline.hpp
        ../common/carray.h
        ../common/cbitset_inline.hpp
        ../common/cbitset.cpp
        ../common/cbitset.h
        ../common/cbuffer_inline.hpp
        ../common/cbuffer.cpp
        ../common/cbuffer.h
        ../common/clinkedlist_inline.hpp
        ../common/clinkedlist.h
        ../common/com_math_inline.hpp
        ../common/com_math.h
        ../common/common_inline.hpp
        ../common/common.cpp
        ../common/common.h
        ../common/constants.h
        ../common/crc32.cpp
        ../common/crc32.h
        ../common/cstring_inline.hpp
        ../common/cstring.cpp
        ../common/cstring.h
        ../common/cstringpool_inline.hpp
        ../common/cstringpool.cpp
        ../common/cstringpool.h
        ../common/entitydata.h
        ../common/hashlist_inline.hpp
        ../common/hashlist.h
        ../common/matrix.cpp
        ../common/matrix.h
        ../common/md5.cpp
        ../common/md5.h
        ../common/msgreader_inline.hpp
        ../common/msgreader.h
        ../common/r_common.cpp
        ../common/r_common.h
        ../common/vector_inline.hpp
        ../common/vector.h
        ../libs/glad/src/glad.c
        ../shared/bike_shared.h
        ../shared/blackhole_shared.h
        ../shared/brushmodel_shared.h
        ../shared/cldll_interface.h
        ../shared/decallist.cpp
        ../shared/decallist.h
        ../shared/efxapi.h
        ../shared/entity_extrainfo.h
        ../shared/flex_shared.h
        ../shared/flexmanager.h
        ../shared/gameui_shared.h
        ../shared/ladder_shared.h
        ../shared/mlight.h
        ../shared/playermove.cpp
        ../shared/playermove.h
        ../shared/r_glsl_inline.hpp
        ../shared/r_glsl.cpp
        ../shared/r_glsl.h
        ../shared/r_interface.h
        ../shared/r_vbo.cpp
        ../shared/r_vbo.h
        ../shared/stepsound.cpp
        ../shared/stepsound.h
        ../shared/uischema.cpp
        ../shared/uischema.h
        ../shared/vbm_shared.cpp
        ../shared/vbm_shared.h
        ../shared/vcontroller_shared.h
        ../shared/weapons_shared.cpp
        ../shared/weapons_shared.h
        animevents.cpp
        animevents.h
        clientdll.cpp
        clientdll.h
        cliententities.cpp
        cliententities.h
        clientusermsgs.cpp
        clientusermsgs.h
        clshared.cpp
        clshared.h
        discordrpc.cpp
        discordrpc.h
        draw.cpp
        draw.h
        entitymanager.cpp
        entitymanager.h
        gameuidocumentswindow.cpp
        gameuidocumentswindow.h
        gameuielements.cpp
        gameuielements.h
        gameuikeypadwindow.cpp
        gameuikeypadwindow.h
        gameuiloginwindow.cpp
        gameuiloginwindow.h
        gameuimanager.cpp
        gameuimanager.h
        gameuiobjectiveswindow.cpp
        gameuiobjectiveswindow.h
        gameuisubwaywindow.cpp
        gameuisubwaywindow.h
        gameuitextwindow.cpp
        gameuitextwindow.h
        gameuiwindows_shared.h
        hud.cpp
        hud.h
        huddraw.cpp
        huddraw.h
        input.cpp
        input.h
        ladder.cpp
        ladder.h
        messages.cpp
        messages.h
        motorbike.cpp
        motorbike.h
        nodedebug.cpp
        nodedebug.h
        saytext.cpp
        saytext.h
        screentext.cpp
        screentext.h
        shake.cpp
        shake.h
        view.cpp
        view.h
        viewcontroller.cpp
        viewcontroller.h
        viewmodel.cpp
        viewmodel.h
)

set_target_properties(client PROPERTIES
    OUTPUT_NAME client_x64
    MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
    MSVC_RUNTIME_CHECKS ""
)

target_include_directories(client
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/glad/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../common"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/discordrpc/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/SDL3-3.4.14/include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../client"
        "${CMAKE_CURRENT_SOURCE_DIR}/../shared"
        "${CMAKE_CURRENT_SOURCE_DIR}/../shared/renderer"
)

target_compile_definitions(client
    PRIVATE
        WIN32
        _64BUILD
        NDEBUG
        _WINDOWS
        _USRDLL
        CLIENT_EXPORTS
        _CRT_SECURE_NO_WARNINGS
        CLIENT
        UNICODE
        _UNICODE
)

target_link_libraries(client
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/SDL3-3.4.14/lib/x64/SDL3.lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/../libs/discordrpc/libs/discord-rpc.lib"
        opengl32
)

target_compile_options(client
    PRIVATE
        "$<$<CXX_COMPILER_ID:MSVC>:/MP>"
        "$<$<CXX_COMPILER_ID:MSVC>:/wd4244>"
        "$<$<CXX_COMPILER_ID:MSVC>:/wd4305>"
        "$<$<CXX_COMPILER_ID:MSVC>:/W3>"
)


source_group("Source Files/common" FILES ../common/cbitset.cpp ../common/cbuffer.cpp ../common/common.cpp ../common/crc32.cpp ../common/cstring.cpp ../common/cstringpool.cpp ../common/matrix.cpp ../common/md5.cpp ../common/r_common.cpp)
source_group("Source Files/libs/glad/src" FILES ../libs/glad/src/glad.c)
source_group("Source Files/shared" FILES ../shared/decallist.cpp ../shared/playermove.cpp ../shared/r_glsl.cpp ../shared/r_vbo.cpp ../shared/stepsound.cpp ../shared/uischema.cpp ../shared/vbm_shared.cpp ../shared/weapons_shared.cpp)
source_group("Source Files" FILES animevents.cpp clientdll.cpp clshared.cpp discordrpc.cpp draw.cpp gameuidocumentswindow.cpp gameuikeypadwindow.cpp gameuiloginwindow.cpp gameuiobjectiveswindow.cpp gameuisubwaywindow.cpp gameuitextwindow.cpp gameuielements.cpp gameuimanager.cpp hud.cpp huddraw.cpp input.cpp cliententities.cpp ladder.cpp messages.cpp motorbike.cpp nodedebug.cpp saytext.cpp screentext.cpp shake.cpp entitymanager.cpp clientusermsgs.cpp view.cpp viewcontroller.cpp viewmodel.cpp)
source_group("Header Files/common" FILES ../common/buttonbits.h ../common/cache_model.h ../common/carray.h ../common/carray_inline.hpp ../common/cbitset.h ../common/cbitset_inline.hpp ../common/cbuffer.h ../common/cbuffer_inline.hpp ../common/clinkedlist.h ../common/clinkedlist_inline.hpp ../common/common.h ../common/common_inline.hpp ../common/com_math.h ../common/com_math_inline.hpp ../common/constants.h ../common/crc32.h ../common/cstring.h ../common/cstringpool.h ../common/cstringpool_inline.hpp ../common/cstring_inline.hpp ../common/entitydata.h ../common/hashlist.h ../common/hashlist_inline.hpp ../common/matrix.h ../common/md5.h ../common/msgreader.h ../common/msgreader_inline.hpp ../common/r_common.h ../common/vector.h ../common/vector_inline.hpp)
source_group("Header Files/shared" FILES ../shared/bike_shared.h ../shared/blackhole_shared.h ../shared/brushmodel_shared.h ../shared/cldll_interface.h ../shared/decallist.h ../shared/entity_extrainfo.h ../shared/flexmanager.h ../shared/flex_shared.h ../shared/ladder_shared.h ../shared/mlight.h ../shared/playermove.h ../shared/efxapi.h ../shared/r_glsl.h ../shared/r_glsl_inline.hpp ../shared/r_interface.h ../shared/r_vbo.h ../shared/stepsound.h ../shared/gameui_shared.h ../shared/uischema.h ../shared/vbm_shared.h ../shared/vcontroller_shared.h ../shared/weapons_shared.h)
source_group("Header Files" FILES animevents.h clientdll.h clshared.h discordrpc.h gameuidocumentswindow.h gameuielements.h draw.h gameuikeypadwindow.h gameuiloginwindow.h gameuiobjectiveswindow.h gameuisubwaywindow.h gameuitextwindow.h gameuimanager.h gameuiwindows_shared.h hud.h huddraw.h input.h cliententities.h ladder.h messages.h motorbike.h nodedebug.h saytext.h screentext.h shake.h entitymanager.h clientusermsgs.h view.h viewcontroller.h viewmodel.h)

add_custom_command(TARGET client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/../../dlls"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:client>" "${CMAKE_SOURCE_DIR}/../../dlls/$<TARGET_FILE_NAME:client>"
)