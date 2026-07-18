# microPOSIX Root CMakeLists.txt
# 
# This file provides the top-level CMake configuration for microPOSIX.
# It can be used with ESP-IDF or as a standalone project.

cmake_minimum_required(VERSION 3.5)

# Project configuration
project(microPOSIX VERSION 1.0.0 LANGUAGES C ASM)

# Set default build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Options
option(MICROPOSIX_PROFILE "Hardware profile (0=Minimal/20MHz, 1=Full/90MHz)" 1)
option(MICROPOSIX_SCHED_PREEMPTIVE "Enable preemptive scheduling" ON)
option(MICROPOSIX_CPU_PROFILE "Enable CPU profiling" ON)
option(MICROPOSIX_LEAK_DETECT "Enable heap leak detection" ON)
option(MICROPOSIX_LCD_ENABLE "Enable LCD display subsystem" ON)
option(MICROPOSIX_SHELL_ENABLE "Enable UART0 debug shell" ON)
option(MICROPOSIX_BLE_ENABLE "Enable BLE subsystem" ON)
option(MICROPOSIX_WIFI_ENABLE "Enable WiFi subsystem" OFF)
option(MICROPOSIX_OTA_ENABLE "Enable OTA updates" OFF)

# Platform detection
if(DEFINED ENV{IDF_PATH})
    # ESP-IDF build
    set(MICROPOSIX_PLATFORM "esp32")
    add_compile_definitions(MICROPOSIX_ESP32=1)
    
    # Detect ESP32 target
    if(CONFIG_IDF_TARGET STREQUAL "esp32")
        add_compile_definitions(MICROPOSIX_ARCH_XTENSA=1)
    elseif(CONFIG_IDF_TARGET STREQUAL "esp32s3" OR CONFIG_IDF_TARGET STREQUAL "esp32c3")
        add_compile_definitions(MICROPOSIX_ARCH_RISCV=1)
    endif()
    
    # ESP-IDF specific settings
    set(CMAKE_C_STANDARD 11)
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    
else()
    # Standalone build - detect platform
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm")
        set(MICROPOSIX_PLATFORM "arm")
        if(CMAKE_SIZEOF_VOID_P EQUAL 4)
            set(MICROPOSIX_ARCH "arm32")
        else()
            set(MICROPOSIX_ARCH "arm64")
        endif()
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "riscv")
        set(MICROPOSIX_PLATFORM "riscv")
        set(MICROPOSIX_ARCH "riscv32")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i686" OR CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
        set(MICROPOSIX_PLATFORM "posix")
        set(MICROPOSIX_ARCH "x86")
    else()
        set(MICROPOSIX_PLATFORM "unknown")
        set(MICROPOSIX_ARCH "unknown")
    endif()
endif()

# Compiler flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Werror -std=c11")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -x assembler-with-cpp")

if(MICROPOSIX_PROFILE EQUAL 1)
    # Full profile (90MHz+)
    add_compile_definitions(MICROPOSIX_PROFILE=1)
    add_compile_definitions(MICROPOSIX_MAX_THREADS=32)
    add_compile_definitions(MICROPOSIX_STACK_SIZE_DEFAULT=1024)
    add_compile_definitions(MICROPOSIX_TICK_RATE_HZ=1000)
else()
    # Minimal profile (20MHz)
    add_compile_definitions(MICROPOSIX_PROFILE=0)
    add_compile_definitions(MICROPOSIX_MAX_THREADS=8)
    add_compile_definitions(MICROPOSIX_STACK_SIZE_DEFAULT=512)
    add_compile_definitions(MICROPOSIX_TICK_RATE_HZ=100)
endif()

# Feature flags
if(MICROPOSIX_SCHED_PREEMPTIVE)
    add_compile_definitions(MICROPOSIX_SCHED_PREEMPTIVE=1)
else()
    add_compile_definitions(MICROPOSIX_SCHED_PREEMPTIVE=0)
endif()

if(MICROPOSIX_CPU_PROFILE)
    add_compile_definitions(MICROPOSIX_CPU_PROFILE=1)
else()
    add_compile_definitions(MICROPOSIX_CPU_PROFILE=0)
endif()

if(MICROPOSIX_LEAK_DETECT)
    add_compile_definitions(MICROPOSIX_LEAK_DETECT=1)
else()
    add_compile_definitions(MICROPOSIX_LEAK_DETECT=0)
endif()

if(MICROPOSIX_LCD_ENABLE)
    add_compile_definitions(MICROPOSIX_LCD_ENABLE=1)
else()
    add_compile_definitions(MICROPOSIX_LCD_ENABLE=0)
endif()

if(MICROPOSIX_SHELL_ENABLE)
    add_compile_definitions(MICROPOSIX_SHELL_ENABLE=1)
else()
    add_compile_definitions(MICROPOSIX_SHELL_ENABLE=0)
endif()

if(MICROPOSIX_BLE_ENABLE)
    add_compile_definitions(MICROPOSIX_BLE_ENABLE=1)
else()
    add_compile_definitions(MICROPOSIX_BLE_ENABLE=0)
endif()

# Include directories
set(MICROPOSIX_INCLUDE_DIRS
    include
    include/microposix
    include/microposix/kernel
    include/microposix/mm
    include/microposix/debug
    include/microposix/hal
    include/microposix/ble
    include/microposix/bootloader
)

# Platform-specific includes
if(MICROPOSIX_PLATFORM STREQUAL "esp32")
    list(APPEND MICROPOSIX_INCLUDE_DIRS include/microposix/hal/esp32)
elseif(MICROPOSIX_PLATFORM STREQUAL "arm")
    list(APPEND MICROPOSIX_INCLUDE_DIRS include/microposix/hal/arm)
elseif(MICROPOSIX_PLATFORM STREQUAL "riscv")
    list(APPEND MICROPOSIX_INCLUDE_DIRS include/microposix/hal/riscv)
elseif(MICROPOSIX_PLATFORM STREQUAL "nrf54l15")
    list(APPEND MICROPOSIX_INCLUDE_DIRS include/microposix/hal/nrf54l15)
endif()

# Source files
set(MICROPOSIX_SRC_KERNEL
    src/kernel/scheduler.c
    src/kernel/thread.c
    src/kernel/ipc.c
    src/kernel/timer.c
    src/kernel/mq.c
    src/kernel/abi.c
    src/kernel/watchdog.c
)

set(MICROPOSIX_SRC_MM
    src/mm/tlsf.c
    src/mm/pool.c
)

set(MICROPOSIX_SRC_DEBUG
    src/debug/log.c
    src/debug/shell.c
    src/debug/fault.c
)

set(MICROPOSIX_SRC_BLE
    src/ble/ble_mgr.c
)

set(MICROPOSIX_SRC_BOOT
    src/bootloader/boot.c
)

# Platform-specific sources
if(MICROPOSIX_PLATFORM STREQUAL "esp32")
    set(MICROPOSIX_SRC_PLATFORM
        platform/esp32/context_switch.c
        platform/esp32/cpu.c
        platform/esp32/gpio.c
        platform/esp32/uart.c
        platform/esp32/wdt.c
        platform/esp32/mpu.c
        src/kernel/abi_esp32.c
    )
    
    # Add BLE backend for ESP32 if enabled
    if(MICROPOSIX_BLE_ENABLE)
        list(APPEND MICROPOSIX_SRC_BLE src/ble/ble_esp32.c)
        list(APPEND MICROPOSIX_INCLUDE_DIRS include/microposix/ble)
    endif()
    
elseif(MICROPOSIX_PLATFORM STREQUAL "arm")
    set(MICROPOSIX_SRC_PLATFORM
        platform/arm/cortex-m4/context_switch.c
        platform/arm/cortex-m4/mpu.c
    )
elseif(MICROPOSIX_PLATFORM STREQUAL "riscv")
    set(MICROPOSIX_SRC_PLATFORM
        platform/riscv/rv32imac/context_switch.c
    )
elseif(MICROPOSIX_PLATFORM STREQUAL "posix")
    set(MICROPOSIX_SRC_PLATFORM
        platform/posix/context_switch.c
    )
elseif(MICROPOSIX_PLATFORM STREQUAL "nrf54l15")
    # Include the nRF54L15 platform CMake file
    include(platform/nrf54l15/CMakeLists.txt)
endif()

# Create microPOSIX library
add_library(microposix
    ${MICROPOSIX_SRC_KERNEL}
    ${MICROPOSIX_SRC_MM}
    ${MICROPOSIX_SRC_DEBUG}
    ${MICROPOSIX_SRC_BLE}
    ${MICROPOSIX_SRC_BOOT}
    ${MICROPOSIX_SRC_PLATFORM}
)

target_include_directories(microposix PRIVATE ${MICROPOSIX_INCLUDE_DIRS})

# Set compiler flags for microPOSIX library
target_compile_options(microposix PRIVATE
    -Wall
    -Wextra
    -Werror
    -std=c11
    -ffunction-sections
    -fdata-sections
)

# Platform-specific compiler flags
if(MICROPOSIX_PLATFORM STREQUAL "esp32")
    target_compile_options(microposix PRIVATE
        -DMICROPOSIX_ESP32=1
    )
    
    if(CONFIG_IDF_TARGET STREQUAL "esp32")
        target_compile_options(microposix PRIVATE
            -DMICROPOSIX_ARCH_XTENSA=1
        )
    elseif(CONFIG_IDF_TARGET STREQUAL "esp32s3" OR CONFIG_IDF_TARGET STREQUAL "esp32c3")
        target_compile_options(microposix PRIVATE
            -DMICROPOSIX_ARCH_RISCV=1
        )
    endif()
elseif(MICROPOSIX_PLATFORM STREQUAL "nrf54l15")
    # Apply core-specific flags from the platform CMake file
    if(DEFINED CORE_C_FLAGS)
        target_compile_options(microposix PRIVATE ${CORE_C_FLAGS})
    endif()
    if(DEFINED CORE_ASM_FLAGS)
        target_compile_options(microposix PRIVATE ${CORE_ASM_FLAGS})
    endif()
    
    # Add nRF54L15-specific definitions
    target_compile_options(microposix PRIVATE
        -DMICROPOSIX_NRF54L15=1
    )
endif()

# Linker flags
target_link_options(microposix PRIVATE
    -ffunction-sections
    -fdata-sections
)

# Platform-specific linker script
if(MICROPOSIX_PLATFORM STREQUAL "nrf54l15")
    if(DEFINED CORE_LINKER_SCRIPT)
        target_link_options(microposix PRIVATE
            -T${CORE_LINKER_SCRIPT}
        )
    endif()
endif()

# Install targets
install(TARGETS microposix
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Demo applications (if building with ESP-IDF)
if(DEFINED ENV{IDF_PATH})
    add_subdirectory(demo/esp32)
endif()

# nRF54L15 demo application
if(MICROPOSIX_PLATFORM STREQUAL "nrf54l15")
    add_subdirectory(demo/nrf54l15)
endif()
