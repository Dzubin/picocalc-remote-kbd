# PicoCalc Remote Keyboard/Mouse (by Thomas Dzubin)
#
# Builds two example firmwares that use the reusable library in remote/:
#
#   picocalc-remote-kbd-example   examples/remote_test.c  - the smallest useful program
#   picocalc-remote-kbd-cursor    demo/cursor_demo.c      - arrow pointer + typing demo
#
# To use the library in YOUR OWN program you do not need this file: copy the
# remote/ folder and add two lines to your CMakeLists.txt, see INTEGRATION.md.
# The desktop relay is built separately, from desktop/ (see README.md).


cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# set(PICO_BOARD pico CACHE STRING "Board type")

# Pull in the Pico SDK (must be before project())
include(pico_sdk_import.cmake)

project(picocalc-remote-kbd C CXX ASM)

pico_sdk_init()

# The version number lives in version.h (one place); the firmware reports it too.
file(STRINGS ${CMAKE_CURRENT_LIST_DIR}/version.h _version_line REGEX "REMOTE_KBD_VERSION")
string(REGEX MATCH "V[0-9]+[.][0-9]+[A-Z]*" REMOTE_KBD_VERSION_TEXT "${_version_line}")

# The library: two lines, exactly what another project would write.
include(remote/remote.cmake)

# Location of the vendored picocalc-text-starter drivers (by Blair Leduc;
# read-only). The library itself needs nothing from it; these examples use its
# south-bridge driver for the PicoCalc's own keyboard and the demo uses its LCD
# driver.
set(STARTER ${CMAKE_CURRENT_LIST_DIR}/picocalc-text-starter-main)

# The chip is part of every output file name (remote-kbd-example-RP2040.uf2 /
# -RP2350.uf2, and the matching .elf/.bin/.hex), so a file always says which
# board it was built for.
if(PICO_PLATFORM MATCHES "^rp2040")
    set(CHIP_NAME RP2040)
elseif(PICO_PLATFORM MATCHES "^rp2350")
    set(CHIP_NAME RP2350)
else()
    set(CHIP_NAME ${PICO_PLATFORM})
endif()

# Every firmware target is finished the same way: name with the chip, extra
# outputs, and the .uf2 copied to the top-level folder so the RP2040 and
# RP2350 builds can sit side by side.
function(finish_firmware target)
    # the output file name is the target name without its "picocalc-" prefix
    string(REGEX REPLACE "^picocalc-" "" output_base ${target})
    set_target_properties(${target} PROPERTIES OUTPUT_NAME ${output_base}-${CHIP_NAME})
    pico_add_extra_outputs(${target})
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                $<TARGET_FILE_DIR:${target}>/$<TARGET_FILE_BASE_NAME:${target}>.uf2
                ${CMAKE_CURRENT_LIST_DIR}/
    )
endfunction()

# --- Example: the smallest useful program ---------------------------------------
add_executable(picocalc-remote-kbd-example examples/remote_test.c)
target_compile_options(picocalc-remote-kbd-example PRIVATE -Wall)
remote_add_to_target(picocalc-remote-kbd-example)
remote_add_picocalc_keys(picocalc-remote-kbd-example ${STARTER}/drivers)
pico_set_program_name(picocalc-remote-kbd-example "picocalc-remote-kbd-example")
pico_set_program_version(picocalc-remote-kbd-example "${REMOTE_KBD_VERSION_TEXT}")
finish_firmware(picocalc-remote-kbd-example)

# --- Demo: arrow pointer and typing ---------------------------------------------
# Uses the vendored starter LCD driver (lcd.c + fonts, by Blair Leduc).
add_executable(picocalc-remote-kbd-cursor
    demo/cursor_demo.c
    demo/screen_model.c
    ${STARTER}/drivers/lcd.c
    ${STARTER}/drivers/font-8x10.c
    ${STARTER}/drivers/font-5x10.c
)
target_compile_options(picocalc-remote-kbd-cursor PRIVATE -Wall)
target_include_directories(picocalc-remote-kbd-cursor PRIVATE ${CMAKE_CURRENT_LIST_DIR}/demo)
remote_add_to_target(picocalc-remote-kbd-cursor)
remote_add_picocalc_keys(picocalc-remote-kbd-cursor ${STARTER}/drivers)
target_link_libraries(picocalc-remote-kbd-cursor hardware_spi)
pico_set_program_name(picocalc-remote-kbd-cursor "picocalc-remote-kbd-cursor")
pico_set_program_version(picocalc-remote-kbd-cursor "${REMOTE_KBD_VERSION_TEXT}")
finish_firmware(picocalc-remote-kbd-cursor)
