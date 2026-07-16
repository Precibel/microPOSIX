#ifndef MICROPOSIX_HAL_ESP32_CPU_H
#define MICROPOSIX_HAL_ESP32_CPU_H

#include <stdint.h>
#include <stdbool.h>

// ESP32 is Xtensa architecture, not ARM
// ESP32-S3 and ESP32-C3 are RISC-V

// For ESP32 (Xtensa)
#if defined(CONFIG_IDF_TARGET_ESP32)
#include "esp32/rom/ets_sys.h"
#include "esp32/rom/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_intr_alloc.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"

// For ESP32-S3/C3 (RISC-V)
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
#include "esp32s3/rom/ets_sys.h"
#include "esp32s3/rom/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_intr_alloc.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_task_wdt.h"

#else
#error "Unsupported ESP32 target"
#endif

// CPU architecture detection
#if defined(CONFIG_IDF_TARGET_ESP32)
#define MICROPOSIX_ARCH_XTENSA 1
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
#define MICROPOSIX_ARCH_RISCV 1
#else
#define MICROPOSIX_ARCH_UNKNOWN 1
#endif

// ESP32 has dual cores (Pro CPU and App CPU)
#define MICROPOSIX_ESP32_DUAL_CORE 1

// CPU frequency (configurable in menuconfig)
#ifndef MICROPOSIX_CPU_FREQ_MHZ
#define MICROPOSIX_CPU_FREQ_MHZ 160  // Default for ESP32
#endif

// Critical section management
// ESP32 uses FreeRTOS, so we wrap its critical section macros

static inline uint32_t mp_hal_esp32_enter_critical(void) {
    // Disable interrupts and return previous state
    // On ESP32 with FreeRTOS, we use taskENTER_CRITICAL
    // But we need to return the previous interrupt state
    uint32_t state = 0;
    __asm__ volatile ("rsr %0, ps" : "=r" (state));
    taskENTER_CRITICAL();
    return state;
}

static inline void mp_hal_esp32_exit_critical(uint32_t state) {
    taskEXIT_CRITICAL();
    __asm__ volatile ("wsr %0, ps" :: "r" (state));
}

// For compatibility with existing microPOSIX code
#define mp_hal_cpu_enter_critical mp_hal_esp32_enter_critical
#define mp_hal_cpu_exit_critical mp_hal_esp32_exit_critical

// CPU reset
static inline void mp_hal_esp32_reset(void) {
    esp_restart();
}

// CPU sleep/wait for interrupt
static inline void mp_hal_esp32_wfi(void) {
    // ESP32 doesn't have a simple WFI instruction like ARM
    // We use FreeRTOS idle task which handles sleep
    vTaskDelay(1);
}

// Get current CPU core ID
static inline int mp_hal_esp32_get_core_id(void) {
    return xPortGetCoreID();
}

// Get cycle count (for CPU profiling)
// ESP32 has a cycle counter in the CPU
static inline uint32_t mp_hal_esp32_get_cycle_count(void) {
    uint32_t cycles;
    __asm__ volatile ("rsr %0, ccount" : "=r" (cycles));
    return cycles;
}

// Initialize CPU-specific features
void mp_hal_esp32_cpu_init(void);

// ESP32-specific: Set CPU frequency
void mp_hal_esp32_set_cpu_freq(uint32_t freq_mhz);

// ESP32-specific: Get CPU frequency
uint32_t mp_hal_esp32_get_cpu_freq(void);

#endif // MICROPOSIX_HAL_ESP32_CPU_H
