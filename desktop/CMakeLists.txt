# PicoCalc Remote Keyboard - desktop relay (SDL2) for Windows and Linux, by Thomas Dzubin.
#
# Builds relay.c, the program that captures this computer's keyboard and mouse
# and sends them to the PicoCalc over a USB serial port.
#
# Build on Windows (MSYS2 UCRT64 shell with SDL2 installed):
#     cmake -S desktop -B build-windows -G Ninja
#     ninja -C build-windows
# Output: remote-kbd-relay-Windows.exe, copied to the top-level
# project folder. SDL2 is linked statically: one self-contained file.
#
# Build on Linux (needs the SDL2 development package, e.g. libsdl2-dev):
#     cmake -S desktop -B build-linux
#     cmake --build build-linux
# Output: remote-kbd-relay-Linux, copied to the top-level project
# folder together with libSDL2-2.0.so.0 (keep the two files side by side; the
# executable finds the library through an $ORIGIN run-path).

cmake_minimum_required(VERSION 3.13)

project(picocalc-remote-kbd-relay C)

set(CMAKE_C_STANDARD 11)

if(WIN32)
    set(PLATFORM_NAME Windows)
else()
    set(PLATFORM_NAME Linux)
endif()
set(APP_NAME remote-kbd-relay-${PLATFORM_NAME})

get_filename_component(TOP ${CMAKE_CURRENT_LIST_DIR}/.. ABSOLUTE)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

set(STARTER_DRIVERS ${TOP}/picocalc-text-starter-main/drivers)

# The relay can draw a live copy of the cursor demo's screen (relay_mirror.c). That is demo
# support: turn it OFF for a relay you ship with your own PicoCalc program, which then just
# shows the status colour. The generic relay works with any program built on remote/.
option(RELAY_WITH_MIRROR "Draw the cursor demo's screen in the relay window" ON)

add_executable(${APP_NAME}
    relay.c
    relay_serial.c
    relay_inject.c
    ${TOP}/remote/hid_keymap.c
)

if(RELAY_WITH_MIRROR)
    target_sources(${APP_NAME} PRIVATE
        relay_mirror.c
        ${TOP}/demo/screen_model.c
        ${STARTER_DRIVERS}/font-8x10.c
    )
    target_compile_definitions(${APP_NAME} PRIVATE RELAY_WITH_MIRROR)
endif()

# This folder comes first so "pico/stdlib.h" finds the stand-in header (needed by the font).
target_include_directories(${APP_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${TOP}/remote ${TOP}/demo ${STARTER_DRIVERS} ${SDL2_INCLUDE_DIRS})
target_compile_definitions(${APP_NAME} PRIVATE SDL_MAIN_HANDLED)
target_compile_options(${APP_NAME} PRIVATE -Wall)

if(WIN32)
    list(REMOVE_ITEM SDL2_STATIC_LIBRARIES SDL2main)
    target_link_directories(${APP_NAME} PRIVATE ${SDL2_STATIC_LIBRARY_DIRS})
    target_link_libraries(${APP_NAME} PRIVATE ${SDL2_STATIC_LIBRARIES} setupapi)
    # sdl2.pc adds -mwindows (a GUI-subsystem exe with no console, so every
    # printf vanishes); the trailing -mconsole keeps this a console program.
    target_link_options(${APP_NAME} PRIVATE -static ${SDL2_STATIC_LDFLAGS_OTHER} -mconsole)
else()
    target_link_directories(${APP_NAME} PRIVATE ${SDL2_LIBRARY_DIRS})
    target_link_libraries(${APP_NAME} PRIVATE ${SDL2_LIBRARIES})
    target_link_options(${APP_NAME} PRIVATE "LINKER:-rpath,$ORIGIN")

    find_library(SDL2_SHARED_LIB NAMES libSDL2-2.0.so.0 SDL2-2.0 SDL2 HINTS ${SDL2_LIBRARY_DIRS})
    if(NOT SDL2_SHARED_LIB)
        message(FATAL_ERROR "libSDL2 shared library not found - install the SDL2 development package (e.g. libsdl2-dev)")
    endif()
    get_filename_component(SDL2_SHARED_REAL "${SDL2_SHARED_LIB}" REALPATH)
    add_custom_command(TARGET ${APP_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "${SDL2_SHARED_REAL}" $<TARGET_FILE_DIR:${APP_NAME}>/libSDL2-2.0.so.0
        COMMAND ${CMAKE_COMMAND} -E copy "${SDL2_SHARED_REAL}" ${TOP}/libSDL2-2.0.so.0
    )
endif()

add_custom_command(TARGET ${APP_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${APP_NAME}> ${TOP}/
)
