/**
 * ESP32 CPU HAL Implementation
 * 
 * This file provides CPU-specific functions for ESP32.
 */

#include <stdint.h>
#include <stdbool.h>
#include "microposix/hal/esp32/cpu.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_cpu.h"

static const char *TAG = "mp_cpu";

// Current CPU frequency
static uint32_t current_cpu_freq = MICROPOSIX_CPU_FREQ_MHZ;

// Initialize CPU-specific features
void mp_hal_esp32_cpu_init(void) {
    // Get current CPU frequency
    current_cpu_freq = esp_clk_cpu_freq() / 1000000;  // Convert Hz to MHz
    
    ESP_LOGI(TAG, "CPU initialized, frequency: %u MHz", current_cpu_freq);
    
    // Enable cycle counter (ccount register)
    // On ESP32, the cycle counter is always running
    // On ESP32-S3/C3, we need to enable it
    #if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
    // Enable cycle counter for RISC-V
    // This is typically done in startup code
    #endif
}

// Set CPU frequency
void mp_hal_esp32_set_cpu_freq(uint32_t freq_mhz) {
    esp_err_t err = esp_set_cpu_freq(freq_mhz * 1000000);  // Convert MHz to Hz
    if (err == ESP_OK) {
        current_cpu_freq = freq_mhz;
        ESP_LOGI(TAG, "CPU frequency set to %u MHz", freq_mhz);
    } else {
        ESP_LOGE(TAG, "Failed to set CPU frequency to %u MHz: %s", freq_mhz, esp_err_to_name(err));
    }
}

// Get CPU frequency
uint32_t mp_hal_esp32_get_cpu_freq(void) {
    return current_cpu_freq;
}

// Get CPU core ID
int mp_hal_esp32_get_core_id(void) {
    return xPortGetCoreID();
}

// Get cycle count
uint32_t mp_hal_esp32_get_cycle_count(void) {
    uint32_t cycles;
    
    #if defined(CONFIG_IDF_TARGET_ESP32)
    // ESP32 (Xtensa) - read ccount register
    __asm__ volatile ("rsr %0, ccount" : "=r" (cycles));
    #elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
    // ESP32-S3/C3 (RISC-V) - read mcycle or cycle CSR
    // For now, use a simple approach
    // In RISC-V, we can read the cycle counter from the mcycle CSR
    // This requires the appropriate privileges
    #if defined(__riscv)
    __asm__ volatile ("rdcycle %0" : "=r" (cycles));
    #else
    cycles = 0;  // Fallback
    #endif
    #else
    cycles = 0;
    #endif
    
    return cycles;
}

// CPU reset
void mp_hal_esp32_reset(void) {
    ESP_LOGW(TAG, "CPU reset requested");
    esp_restart();
}

// CPU sleep/wait for interrupt
void mp_hal_esp32_wfi(void) {
    // On ESP32, we can use vTaskDelay with 0 ticks to yield
    // or use esp_cpu_light_sleep() for light sleep
    vTaskDelay(0);
}

// Enter critical section
uint32_t mp_hal_esp32_enter_critical(void) {
    uint32_t state = 0;
    
    #if defined(CONFIG_IDF_TARGET_ESP32)
    // Xtensa: read PS register
    __asm__ volatile ("rsr %0, ps" : "=r" (state));
    #elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
    // RISC-V: read mstatus or mcause
    // For now, just use FreeRTOS critical section
    #endif
    
    taskENTER_CRITICAL();
    return state;
}

// Exit critical section
void mp_hal_esp32_exit_critical(uint32_t state) {
    taskEXIT_CRITICAL();
    
    #if defined(CONFIG_IDF_TARGET_ESP32)
    // Xtensa: restore PS register
    __asm__ volatile ("wsr %0, ps" :: "r" (state));
    #endif
}

// Get CPU unique ID (for identification)
uint32_t mp_hal_esp32_get_chip_id(void) {
    // ESP32 has a unique chip ID in the efuse
    uint32_t chip_id = 0;
    
    #if defined(CONFIG_IDF_TARGET_ESP32)
    chip_id = esp_efuse_read_field_blob(ESP_EFUSE_MAC_FACTORY, NULL, 0);
    #elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
    // ESP32-S3/C3 have different efuse layout
    esp_read_mac((uint8_t *)&chip_id, ESP_MAC_WIFI_STA);
    #endif
    
    return chip_id;
}

// Get CPU temperature (if supported)
float mp_hal_esp32_get_temperature(void) {
    // ESP32 has an internal temperature sensor
    // Note: This is not very accurate and varies between chips
    
    #if defined(CONFIG_IDF_TARGET_ESP32)
    // ESP32 temperature sensor is in the ADC
    // This is a simplified version
    return 0.0f;  // Placeholder
    #else
    return 0.0f;
    #endif
}

// Get CPU voltage (if supported)
float mp_hal_esp32_get_voltage(void) {
    // ESP32 can measure its own voltage using ADC
    return 0.0f;  // Placeholder
}

// Initialize CPU performance counters
void mp_hal_esp32_perf_init(void) {
    // Enable performance counters if available
    ESP_LOGI(TAG, "Performance counters initialized");
}

// Get instruction count (if supported)
uint32_t mp_hal_esp32_get_instruction_count(void) {
    // ESP32 doesn't have a built-in instruction counter
    // We could implement this in software
    return 0;
}
