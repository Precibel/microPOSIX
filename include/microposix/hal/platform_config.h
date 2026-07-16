#ifndef MICROPOSIX_HAL_PLATFORM_CONFIG_H
#define MICROPOSIX_HAL_PLATFORM_CONFIG_H

/**
 * Platform Configuration Header
 * 
 * This header provides platform-specific configuration and macros
 * for different microcontroller architectures.
 */

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// Platform Detection
// ============================================================================

// Check for ESP32 platform
#if defined(CONFIG_IDF_TARGET_ESP32) || defined(MICROPOSIX_ESP32)
#define MICROPOSIX_PLATFORM_ESP32 1

#if defined(CONFIG_IDF_TARGET_ESP32)
#define MICROPOSIX_ARCH_XTENSA 1
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
#define MICROPOSIX_ARCH_RISCV 1
#else
#define MICROPOSIX_ARCH_XTENSA 1  // Default for ESP32
#endif

// Check for ARM platform
#elif defined(__ARM_ARCH_7EM__) || defined(__ARM_ARCH_7EM__) || defined(MICROPOSIX_PLATFORM_ARM)
#define MICROPOSIX_PLATFORM_ARM 1

#if defined(__ARM_ARCH_7EM__)
#define MICROPOSIX_ARCH_ARM_CORTEX_M4 1
#elif defined(__ARM_ARCH_6M__)
#define MICROPOSIX_ARCH_ARM_CORTEX_M0PLUS 1
#else
#define MICROPOSIX_ARCH_ARM_CORTEX_M4 1  // Default
#endif

// Check for RISC-V platform
#elif defined(__riscv) || defined(MICROPOSIX_PLATFORM_RISCV)
#define MICROPOSIX_PLATFORM_RISCV 1
#define MICROPOSIX_ARCH_RISCV 1

// Check for POSIX platform (testing)
#elif defined(__linux__) || defined(__APPLE__) || defined(MICROPOSIX_PLATFORM_POSIX)
#define MICROPOSIX_PLATFORM_POSIX 1

// Unknown platform
#else
#error "Unknown platform - please define MICROPOSIX_PLATFORM_*"
#endif

// ============================================================================
// Profile Configuration
// ============================================================================

// Profile selection (0 = Minimal, 1 = Full)
#ifndef MICROPOSIX_PROFILE
#define MICROPOSIX_PROFILE 1
#endif

#if MICROPOSIX_PROFILE == 1
// Full profile (90MHz+)
#define MICROPOSIX_MAX_THREADS 32
#define MICROPOSIX_STACK_SIZE_DEFAULT 1024
#define MICROPOSIX_TICK_RATE_HZ 1000
#define MICROPOSIX_CPU_PROFILE_ENABLED 1
#define MICROPOSIX_LEAK_DETECT_ENABLED 1
#define MICROPOSIX_LCD_ENABLED 1
#define MICROPOSIX_SHELL_ENABLED 1
#else
// Minimal profile (20MHz)
#define MICROPOSIX_MAX_THREADS 8
#define MICROPOSIX_STACK_SIZE_DEFAULT 512
#define MICROPOSIX_TICK_RATE_HZ 100
#define MICROPOSIX_CPU_PROFILE_ENABLED 0
#define MICROPOSIX_LEAK_DETECT_ENABLED 0
#define MICROPOSIX_LCD_ENABLED 0
#define MICROPOSIX_SHELL_ENABLED 0
#endif

// ============================================================================
// Architecture-Specific Configuration
// ============================================================================

// ARM Cortex-M4 specific
#if defined(MICROPOSIX_ARCH_ARM_CORTEX_M4)
#define MICROPOSIX_HAS_FPU 1
#define MICROPOSIX_HAS_DWT 1
#define MICROPOSIX_HAS_MPU 1
#define MICROPOSIX_HAS_SVC 1
#define MICROPOSIX_STACK_ALIGNMENT 8
#define MICROPOSIX_INTERRUPT_COUNT 240

// ARM Cortex-M0+ specific
#elif defined(MICROPOSIX_ARCH_ARM_CORTEX_M0PLUS)
#define MICROPOSIX_HAS_FPU 0
#define MICROPOSIX_HAS_DWT 0
#define MICROPOSIX_HAS_MPU 0
#define MICROPOSIX_HAS_SVC 0
#define MICROPOSIX_STACK_ALIGNMENT 4
#define MICROPOSIX_INTERRUPT_COUNT 32

// Xtensa (ESP32) specific
#elif defined(MICROPOSIX_ARCH_XTENSA)
#define MICROPOSIX_HAS_FPU 0
#define MICROPOSIX_HAS_DWT 0
#define MICROPOSIX_HAS_MPU 1
#define MICROPOSIX_HAS_SVC 0
#define MICROPOSIX_STACK_ALIGNMENT 4
#define MICROPOSIX_INTERRUPT_COUNT 32
#define MICROPOSIX_HAS_DUAL_CORE 1

// RISC-V specific
#elif defined(MICROPOSIX_ARCH_RISCV)
#define MICROPOSIX_HAS_FPU 0
#define MICROPOSIX_HAS_DWT 0
#define MICROPOSIX_HAS_MPU 0
#define MICROPOSIX_HAS_PMP 1
#define MICROPOSIX_HAS_SVC 0
#define MICROPOSIX_STACK_ALIGNMENT 4
#define MICROPOSIX_INTERRUPT_COUNT 32
#define MICROPOSIX_HAS_DUAL_CORE 1

// POSIX (testing) specific
#elif defined(MICROPOSIX_PLATFORM_POSIX)
#define MICROPOSIX_HAS_FPU 0
#define MICROPOSIX_HAS_DWT 0
#define MICROPOSIX_HAS_MPU 0
#define MICROPOSIX_HAS_SVC 0
#define MICROPOSIX_STACK_ALIGNMENT 4
#define MICROPOSIX_INTERRUPT_COUNT 0

#endif

// ============================================================================
// ABI Configuration
// ============================================================================

// ABI version
#define MICROPOSIX_ABI_MAGIC 0xDEADBEEF
#define MICROPOSIX_ABI_VERSION 0x01000000  // v1.0.0

// ABI mechanism selection
#if defined(MICROPOSIX_HAS_SVC) && MICROPOSIX_HAS_SVC
// Use SVC router for Cortex-M33
#define MICROPOSIX_ABI_USE_SVC 1
#elif defined(MICROPOSIX_PLATFORM_ESP32)
// Use jump table for ESP32
#define MICROPOSIX_ABI_USE_JUMP_TABLE 1
#elif defined(MICROPOSIX_ARCH_ARM_CORTEX_M0PLUS)
// Use jump table for Cortex-M0+
#define MICROPOSIX_ABI_USE_JUMP_TABLE 1
#else
// Default to jump table
#define MICROPOSIX_ABI_USE_JUMP_TABLE 1
#endif

// Jump table address
#if defined(MICROPOSIX_PLATFORM_ESP32)
// ESP32 - place in IRAM
#define MICROPOSIX_SYS_TABLE_ADDR 0x3FFE0000
#elif defined(MICROPOSIX_ARCH_ARM_CORTEX_M33)
// Cortex-M33 (CC2755)
#define MICROPOSIX_SYS_TABLE_ADDR 0x00077E00
#elif defined(MICROPOSIX_ARCH_ARM_CORTEX_M0PLUS)
// Cortex-M0+ (CC2340R5)
#define MICROPOSIX_SYS_TABLE_ADDR 0x0003FE00
#else
// Default
#define MICROPOSIX_SYS_TABLE_ADDR 0x0003FE00
#endif

// ============================================================================
// Memory Configuration
// ============================================================================

// Stack sizes
#define MICROPOSIX_IDLE_STACK_SIZE 256
#define MICROPOSIX_SHELL_STACK_SIZE 1024
#define MICROPOSIX_BLE_STACK_SIZE 2048

// Heap sizes
#define MICROPOSIX_TLSF_POOL_SIZE (64 * 1024)  // 64KB
#define MICROPOSIX_POOL_BLOCK_SIZE 32
#define MICROPOSIX_POOL_BLOCK_COUNT 1024

// ============================================================================
// Hardware Configuration
// ============================================================================

// UART configuration
#define MICROPOSIX_UART_SHELL_PORT 0
#define MICROPOSIX_UART_SHELL_BAUD 115200
#define MICROPOSIX_UART_BUFFER_SIZE 4096

// GPIO configuration
#if defined(MICROPOSIX_PLATFORM_ESP32)
#define MICROPOSIX_GPIO_LED_BUILTIN 2  // GPIO 2 for most ESP32 boards
#elif defined(MICROPOSIX_PLATFORM_ARM)
#define MICROPOSIX_GPIO_LED_BUILTIN 13  // Example for ARM
#else
#define MICROPOSIX_GPIO_LED_BUILTIN 0
#endif

// Watchdog configuration
#define MICROPOSIX_WDT_TIMEOUT_MS 5000  // 5 seconds

// BLE configuration
#define MICROPOSIX_BLE_MAX_CONNECTIONS 4
#define MICROPOSIX_BLE_MTU 247
#define MICROPOSIX_BLE_CONN_INTERVAL_MIN 0x0006  // 7.5ms
#define MICROPOSIX_BLE_CONN_INTERVAL_MAX 0x000C  // 15ms

// ============================================================================
// Feature Toggles
// ============================================================================

// Enable/disable features based on profile
#if MICROPOSIX_PROFILE == 1
#define MICROPOSIX_ENABLE_CPU_PROFILE 1
#define MICROPOSIX_ENABLE_LEAK_DETECT 1
#define MICROPOSIX_ENABLE_SHELL 1
#define MICROPOSIX_ENABLE_LCD 1
#define MICROPOSIX_ENABLE_BLE 1
#else
#define MICROPOSIX_ENABLE_CPU_PROFILE 0
#define MICROPOSIX_ENABLE_LEAK_DETECT 0
#define MICROPOSIX_ENABLE_SHELL 0
#define MICROPOSIX_ENABLE_LCD 0
#define MICROPOSIX_ENABLE_BLE 0
#endif

// ============================================================================
// Utility Macros
// ============================================================================

// Stringify macro
#define MICROPOSIX_STRINGIFY(x) #x
#define MICROPOSIX_TOSTRING(x) MICROPOSIX_STRINGIFY(x)

// Concatenate macros
#define MICROPOSIX_CONCAT(a, b) a##b

// Minimum/Maximum
#define MICROPOSIX_MIN(a, b) ((a) < (b) ? (a) : (b))
#define MICROPOSIX_MAX(a, b) ((a) > (b) ? (a) : (b))

// Array size
#define MICROPOSIX_ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// Container of
#define MICROPOSIX_CONTAINER_OF(ptr, type, member) \
    ((type *)((char *)(ptr) - offsetof(type, member)))

// ============================================================================
// Platform-Specific Includes
// ============================================================================

// Include platform-specific headers
#if defined(MICROPOSIX_PLATFORM_ESP32)
#include "microposix/hal/esp32/cpu.h"
#include "microposix/hal/esp32/gpio.h"
#include "microposix/hal/esp32/uart.h"
#include "microposix/hal/esp32/wdt.h"
#include "microposix/hal/esp32/mpu.h"
#elif defined(MICROPOSIX_PLATFORM_ARM)
#include "microposix/hal/arm/cpu.h"
#include "microposix/hal/arm/mpu.h"
#elif defined(MICROPOSIX_PLATFORM_RISCV)
#include "microposix/hal/riscv/cpu.h"
#endif

#endif // MICROPOSIX_HAL_PLATFORM_CONFIG_H
