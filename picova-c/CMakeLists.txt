cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(picova)
pico_sdk_init()

add_executable(picova
    main.c
    ina219.c
)

target_link_libraries(picova 
    hardware_gpio
    hardware_i2c
    pico_runtime
    pico_stdio
    pico_stdio_usb
    pico_printf
    pico_multicore
)

pico_add_extra_outputs(picova)
