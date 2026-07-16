#ifndef MICROPOSIX_HAL_ESP32_MPU_H
#define MICROPOSIX_HAL_ESP32_MPU_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_memory_utils.h"

// Note: ESP32 (Xtensa) and ESP32-S3/C3 (RISC-V) have different MPU implementations

// ESP32 (Xtensa) has a simple MPU with 8 regions
// ESP32-S3/C3 (RISC-V) has a more sophisticated PMP (Physical Memory Protection)

// For ESP32, we'll use the MPU in the RTC controller
// For ESP32-S3/C3, we'll use the PMP

// MPU region configuration
typedef struct {
    uint32_t base_addr;      // Base address of the region
    uint32_t size;           // Size of the region (must be power of 2)
    uint32_t permissions;    // Read/Write/Execute permissions
    bool enable;             // Whether the region is enabled
} mp_hal_esp32_mpu_region_t;

// MPU permissions
typedef enum {
    MP_HAL_ESP32_MPU_NO_ACCESS = 0,      // No access
    MP_HAL_ESP32_MPU_READ_ONLY = 1,      // Read only
    MP_HAL_ESP32_MPU_READ_WRITE = 2,     // Read and write
    MP_HAL_ESP32_MPU_READ_EXECUTE = 3,   // Read and execute
    MP_HAL_ESP32_MPU_FULL_ACCESS = 7     // Read, write, execute
} mp_hal_esp32_mpu_perm_t;

// Initialize MPU
int mp_hal_esp32_mpu_init(void);

// Enable MPU
void mp_hal_esp32_mpu_enable(void);

// Disable MPU
void mp_hal_esp32_mpu_disable(void);

// Configure a single MPU region
int mp_hal_esp32_mpu_configure_region(uint32_t region_idx, 
                                     uint32_t base_addr, uint32_t size,
                                     mp_hal_esp32_mpu_perm_t permissions);

// Configure multiple MPU regions
int mp_hal_esp32_mpu_configure(const mp_hal_esp32_mpu_region_t *regions, uint32_t num_regions);

// Set memory protection for application code
int mp_hal_esp32_mpu_set_app_code_protection(uint32_t start_addr, uint32_t size);

// Set memory protection for application data
int mp_hal_esp32_mpu_set_app_data_protection(uint32_t start_addr, uint32_t size);

// Set memory protection for OS code
int mp_hal_esp32_mpu_set_os_protection(uint32_t start_addr, uint32_t size);

// Set stack guard region (to catch stack overflow)
int mp_hal_esp32_mpu_set_stack_guard(uint32_t stack_base, uint32_t stack_size);

// Check if MPU is supported on this chip
bool mp_hal_esp32_mpu_is_supported(void);

// ESP32-specific: Use cache attributes for memory regions
void mp_hal_esp32_mpu_set_cache_attributes(uint32_t region_idx, bool cacheable);

// ESP32-S3/C3 specific: PMP (Physical Memory Protection) functions
#if defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)

// PMP region configuration for RISC-V
int mp_hal_esp32_pmp_configure_region(uint32_t region_idx,
                                      uint32_t base_addr, uint32_t size,
                                      mp_hal_esp32_mpu_perm_t permissions);

#endif

#endif // MICROPOSIX_HAL_ESP32_MPU_H
