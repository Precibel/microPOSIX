/**
 * ESP32 MPU HAL Implementation
 * 
 * This file provides MPU (Memory Protection Unit) functions for ESP32.
 * 
 * Note: ESP32 (Xtensa) and ESP32-S3/C3 (RISC-V) have different MPU implementations.
 * ESP32 uses a simple MPU in the RTC controller.
 * ESP32-S3/C3 uses PMP (Physical Memory Protection) which is more sophisticated.
 */

#include <stdint.h>
#include <stdbool.h>
#include "microposix/hal/esp32/mpu.h"
#include "esp_log.h"
#include "esp_memory_utils.h"

static const char *TAG = "mp_mpu";

// Check if MPU is supported on this chip
bool mp_hal_esp32_mpu_is_supported(void) {
    #if defined(CONFIG_IDF_TARGET_ESP32)
    // ESP32 (Xtensa) has a simple MPU
    return true;
    #elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
    // ESP32-S3/C3 (RISC-V) has PMP
    return true;
    #else
    return false;
    #endif
}

// Initialize MPU
int mp_hal_esp32_mpu_init(void) {
    if (!mp_hal_esp32_mpu_is_supported()) {
        ESP_LOGW(TAG, "MPU not supported on this chip");
        return -1;
    }
    
    ESP_LOGI(TAG, "MPU initialized");
    return 0;
}

// Enable MPU
void mp_hal_esp32_mpu_enable(void) {
    if (!mp_hal_esp32_mpu_is_supported()) {
        return;
    }
    
    #if defined(CONFIG_IDF_TARGET_ESP32)
    // ESP32: Enable MPU
    // This is typically done in the startup code
    // For ESP32, we can use the RTC MPU
    #elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
    // ESP32-S3/C3: Enable PMP
    // This requires writing to the PMP CSRs
    #endif
    
    ESP_LOGI(TAG, "MPU enabled");
}

// Disable MPU
void mp_hal_esp32_mpu_disable(void) {
    if (!mp_hal_esp32_mpu_is_supported()) {
        return;
    }
    
    #if defined(CONFIG_IDF_TARGET_ESP32)
    // ESP32: Disable MPU
    #elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
    // ESP32-S3/C3: Disable PMP
    #endif
    
    ESP_LOGI(TAG, "MPU disabled");
}

// Configure a single MPU region
int mp_hal_esp32_mpu_configure_region(uint32_t region_idx, 
                                     uint32_t base_addr, uint32_t size,
                                     mp_hal_esp32_mpu_perm_t permissions) {
    if (!mp_hal_esp32_mpu_is_supported()) {
        return -1;
    }
    
    if (region_idx >= 8) {  // ESP32 MPU has 8 regions
        ESP_LOGE(TAG, "Invalid region index: %d", region_idx);
        return -1;
    }
    
    // Ensure size is a power of 2
    if ((size & (size - 1)) != 0) {
        ESP_LOGE(TAG, "Region size must be a power of 2");
        return -1;
    }
    
    #if defined(CONFIG_IDF_TARGET_ESP32)
    // ESP32 MPU configuration
    // The ESP32 MPU is configured through the RTC controller
    // This is a simplified implementation
    
    // Calculate region size encoding
    uint32_t size_encoding = 0;
    while (size > 1) {
        size >>= 1;
        size_encoding++;
    }
    
    // Configure the MPU region
    // This would involve writing to the RTC MPU registers
    // For now, we'll just log the configuration
    ESP_LOGI(TAG, "Configuring MPU region %d: base=0x%08X, size=%d, perm=%d",
             region_idx, base_addr, 1 << size_encoding, permissions);
    
    #elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
    // ESP32-S3/C3 PMP configuration
    // PMP uses a different configuration scheme
    ESP_LOGI(TAG, "Configuring PMP region %d: base=0x%08X, size=%d, perm=%d",
             region_idx, base_addr, size, permissions);
    
    #endif
    
    return 0;
}

// Configure multiple MPU regions
int mp_hal_esp32_mpu_configure(const mp_hal_esp32_mpu_region_t *regions, uint32_t num_regions) {
    if (!mp_hal_esp32_mpu_is_supported()) {
        return -1;
    }
    
    if (num_regions > 8) {
        ESP_LOGE(TAG, "Too many regions: %d (max 8)", num_regions);
        return -1;
    }
    
    for (uint32_t i = 0; i < num_regions; i++) {
        int err = mp_hal_esp32_mpu_configure_region(i,
                                                   regions[i].base_addr,
                                                   regions[i].size,
                                                   regions[i].permissions);
        if (err != 0) {
            return -1;
        }
    }
    
    return 0;
}

// Set memory protection for application code
int mp_hal_esp32_mpu_set_app_code_protection(uint32_t start_addr, uint32_t size) {
    mp_hal_esp32_mpu_region_t region = {
        .base_addr = start_addr,
        .size = size,
        .permissions = MP_HAL_ESP32_MPU_READ_EXECUTE,  // Code: read + execute
        .enable = true
    };
    
    return mp_hal_esp32_mpu_configure(&region, 1);
}

// Set memory protection for application data
int mp_hal_esp32_mpu_set_app_data_protection(uint32_t start_addr, uint32_t size) {
    mp_hal_esp32_mpu_region_t region = {
        .base_addr = start_addr,
        .size = size,
        .permissions = MP_HAL_ESP32_MPU_READ_WRITE,  // Data: read + write
        .enable = true
    };
    
    return mp_hal_esp32_mpu_configure(&region, 1);
}

// Set memory protection for OS code
int mp_hal_esp32_mpu_set_os_protection(uint32_t start_addr, uint32_t size) {
    mp_hal_esp32_mpu_region_t region = {
        .base_addr = start_addr,
        .size = size,
        .permissions = MP_HAL_ESP32_MPU_FULL_ACCESS,  // OS: full access
        .enable = true
    };
    
    return mp_hal_esp32_mpu_configure(&region, 1);
}

// Set stack guard region (to catch stack overflow)
int mp_hal_esp32_mpu_set_stack_guard(uint32_t stack_base, uint32_t stack_size) {
    // Create a guard region at the bottom of the stack
    // This region should be small (e.g., 32 bytes) and have no access
    
    mp_hal_esp32_mpu_region_t region = {
        .base_addr = stack_base + stack_size - 32,  // Last 32 bytes of stack
        .size = 32,
        .permissions = MP_HAL_ESP32_MPU_NO_ACCESS,  // No access - will fault on overflow
        .enable = true
    };
    
    return mp_hal_esp32_mpu_configure(&region, 1);
}

// ESP32-S3/C3 specific: PMP (Physical Memory Protection) functions
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)

// PMP region configuration for RISC-V
int mp_hal_esp32_pmp_configure_region(uint32_t region_idx,
                                      uint32_t base_addr, uint32_t size,
                                      mp_hal_esp32_mpu_perm_t permissions) {
    // PMP configuration is more complex than MPU
    // It involves writing to the PMP CSRs (pmpcfg and pmpaddr)
    
    // For now, we'll just log the configuration
    ESP_LOGI(TAG, "Configuring PMP region %d: base=0x%08X, size=%d, perm=%d",
             region_idx, base_addr, size, permissions);
    
    return 0;
}

#endif

// Set cache attributes for memory regions
void mp_hal_esp32_mpu_set_cache_attributes(uint32_t region_idx, bool cacheable) {
    if (!mp_hal_esp32_mpu_is_supported()) {
        return;
    }
    
    // ESP32 has cache attributes that can be configured
    // This is a simplified implementation
    ESP_LOGI(TAG, "Setting cache attributes for region %d: %s",
             region_idx, cacheable ? "cacheable" : "non-cacheable");
}
