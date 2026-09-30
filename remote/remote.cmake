# remote.cmake - picocalc-remote-kbd, CMake helper for using the library
#
# Two lines in your own CMakeLists.txt (after project() and pico_sdk_init()):
#
#     include(path/to/remote/remote.cmake)
#     remote_add_to_target(my_program)
#
# That adds the library sources, the include path and the libraries it needs
# to your target. The path is to the "remote" folder you copied into your
# project. See INTEGRATION.md.
#
# Optional extra: remote_add_picocalc_keys(my_program path/to/starter/drivers)
# also adds picocalc_keys.c, which reads the PicoCalc's own keyboard as USB HID
# key events (and gives you the '~' = reboot-to-BOOTSEL shortcut). It needs the
# picocalc-text-starter "drivers" folder for southbridge.c / keyboard.h.
#
# Author: Thomas Dzubin

set(REMOTE_DIR ${CMAKE_CURRENT_LIST_DIR})

function(remote_add_to_target target)
    target_sources(${target} PRIVATE
        ${REMOTE_DIR}/remote_input.c
        ${REMOTE_DIR}/usb_descriptors.c
        ${REMOTE_DIR}/hid_keymap.c
    )
    target_include_directories(${target} PRIVATE ${REMOTE_DIR})
    target_link_libraries(${target} pico_stdlib pico_unique_id tinyusb_device)

    # The library owns the USB port, so the SDK's own stdio over USB must be off.
    pico_enable_stdio_usb(${target} 0)
endfunction()

function(remote_add_picocalc_keys target starter_drivers_dir)
    target_sources(${target} PRIVATE
        ${REMOTE_DIR}/picocalc_keys.c
        ${starter_drivers_dir}/southbridge.c
    )
    target_include_directories(${target} PRIVATE ${starter_drivers_dir})
    target_link_libraries(${target} hardware_i2c hardware_gpio pico_multicore)
endfunction()
