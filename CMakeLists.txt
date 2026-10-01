# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================

cmake_minimum_required(VERSION 3.13)
set(PICO_BOARD pico)
set(PICO_SDK_PATH ${CMAKE_CURRENT_LIST_DIR}/lib/pico-sdk)
include(pico_sdk_import.cmake)

project(solder_station C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

# FreeRTOS kernel (RP2040 port). Must come after pico_sdk_init().
include(${CMAKE_CURRENT_LIST_DIR}/lib/FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

add_executable(solder_station
    src/main.c
    src/hooks.c
    src/heater.c
    src/control_task.c
)

target_include_directories(solder_station PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/config
    ${CMAKE_CURRENT_LIST_DIR}/include
)

target_link_libraries(solder_station
    pico_stdlib
    hardware_pwm
    FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4
)

pico_enable_stdio_usb(solder_station 1)
pico_enable_stdio_uart(solder_station 0)
pico_add_extra_outputs(solder_station)