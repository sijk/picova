cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(picova)
pico_sdk_init()

set(FREERTOS_KERNEL_PATH ${PROJECT_SOURCE_DIR}/lib/freertos)
set(FREERTOS_CONFIG_FILE_DIRECTORY ${PROJECT_SOURCE_DIR})
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

add_subdirectory(lib/u8g2)
target_compile_definitions(u8g2 PUBLIC U8X8_WITH_USER_PTR)
target_compile_options(u8g2 PUBLIC -ffunction-sections -fdata-sections)
target_link_options(u8g2 PUBLIC LINKER:--gc-sections)

add_subdirectory(lib/pico-i2c-dma)

add_executable(picova
    main.c
    ina219.c
    display.c
)

target_link_libraries(picova
    hardware_gpio
    hardware_i2c
    pico_runtime
    pico_stdio
    pico_stdio_usb
    pico_printf
    pico_multicore
    FreeRTOS-Kernel-Heap3
    i2c_dma
    u8g2
)

pico_add_extra_outputs(picova)
