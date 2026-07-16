# microPOSIX ESP32 Demo - Blinking LED
# This file is used by ESP-IDF build system

cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(microposix_esp32_demo)

# Set the project version
set(PROJECT_VER "1.0.0")

# Include microPOSIX source files
set(MICROPOSIX_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/kernel/scheduler.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/kernel/thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/kernel/ipc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/kernel/timer.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/kernel/mq.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/kernel/abi.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/kernel/watchdog.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/mm/tlsf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/mm/pool.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/debug/log.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/debug/shell.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/debug/fault.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/ble/ble_mgr.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../src/bootloader/boot.c
)

# Include platform-specific files for ESP32
set(PLATFORM_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/esp32/context_switch.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/esp32/cpu.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/esp32/gpio.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/esp32/uart.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/esp32/wdt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/../../platform/esp32/mpu.c
)

# Demo application source files
set(DEMO_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/app_blink.c
)

# Include directories
set(INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/microposix
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/microposix/kernel
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/microposix/mm
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/microposix/debug
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/microposix/hal
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/microposix/hal/esp32
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/microposix/ble
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include/microposix/bootloader
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Add all source files
set(COMPONENT_SRCS ${MICROPOSIX_SRC} ${PLATFORM_SRC} ${DEMO_SRC})

# Add all include directories
set(COMPONENT_INCLUDE_DIRS ${INCLUDE_DIRS})

# Required IDF components
set(COMPONENT_REQUIRES driver freertos esp_common)

# Compiler definitions
set(COMPONENT_DEFINITIONS 
    MICROPOSIX_PROFILE=1
    MICROPOSIX_ESP32=1
    MICROPOSIX_ARCH_XTENSA=1
)

# Compiler flags
set(COMPONENT_CFLAGS "-Wall -Wextra -Werror -std=c11")

# Linker flags
set(COMPONENT_LDFLAGS "")

# Additional linker scripts or libraries
# set(COMPONENT_LDSCRIPT "")

# Set the target to build
set(COMPONENT_EMBED_FILES "")

# Enable this component
set(COMPONENT_ENABLED ON)
