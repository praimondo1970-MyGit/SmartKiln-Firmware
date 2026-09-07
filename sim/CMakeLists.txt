cmake_minimum_required(VERSION 3.20)
project(smartkiln_sim C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(LV_CONF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h CACHE STRING "" FORCE)
set(LV_CONF_BUILD_DISABLE_EXAMPLES ON CACHE BOOL "" FORCE)
set(LV_CONF_BUILD_DISABLE_DEMOS ON CACHE BOOL "" FORCE)

include(FetchContent)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    SDL2
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-2.30.10
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(SDL2)

FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG v9.2.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(lvgl)

if(TARGET lvgl)
    target_compile_definitions(lvgl PUBLIC
        LV_USE_DRAW_SW_ASM=0
        LV_USE_NATIVE_HELIUM_ASM=0
    )
    get_target_property(_lvgl_sources lvgl SOURCES)
    set(_lvgl_sources_filtered "")
    foreach(_src IN LISTS _lvgl_sources)
        if(_src MATCHES "[/\\\\]helium[/\\\\].*\\.S$" OR _src MATCHES "[/\\\\]neon[/\\\\].*\\.S$")
            continue()
        endif()
        list(APPEND _lvgl_sources_filtered "${_src}")
    endforeach()
    set_target_properties(lvgl PROPERTIES SOURCES "${_lvgl_sources_filtered}")
    if(TARGET SDL2::SDL2-static)
        target_link_libraries(lvgl PUBLIC SDL2::SDL2-static)
    elseif(TARGET SDL2::SDL2)
        target_link_libraries(lvgl PUBLIC SDL2::SDL2)
    endif()
endif()

file(GLOB SIM_FONTS "${CMAKE_CURRENT_SOURCE_DIR}/../src/fonts/*.c")
file(GLOB SIM_ICONS "${CMAKE_CURRENT_SOURCE_DIR}/generated/icons/*.c")

if(NOT SIM_ICONS)
    message(FATAL_ERROR "Run: python sim/tools/emit_icons_lv9.py")
endif()

add_executable(smartkiln_sim
    main.c
    display_ui.c
    ${SIM_FONTS}
    ${SIM_ICONS}
)

target_include_directories(smartkiln_sim PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_definitions(smartkiln_sim PRIVATE SDL_MAIN_HANDLED)

if(TARGET lvgl::lvgl)
    target_link_libraries(smartkiln_sim PRIVATE lvgl::lvgl)
else()
    target_link_libraries(smartkiln_sim PRIVATE lvgl)
endif()

if(TARGET SDL2::SDL2-static)
    target_link_libraries(smartkiln_sim PRIVATE SDL2::SDL2-static)
elseif(TARGET SDL2::SDL2)
    target_link_libraries(smartkiln_sim PRIVATE SDL2::SDL2)
endif()

if(WIN32)
    target_link_libraries(smartkiln_sim PRIVATE imm32 version winmm setupapi)
endif()
