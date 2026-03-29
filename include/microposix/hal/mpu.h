#ifndef MICROPOSIX_MPU_H
#define MICROPOSIX_MPU_H

#include <stdint.h>
#include <stdbool.h>

// MPU Region Identifiers
#define MPU_REGION_KERNEL_CODE  0
#define MPU_REGION_KERNEL_DATA  1
#define MPU_REGION_APP_CODE     2
#define MPU_REGION_APP_DATA     3
#define MPU_REGION_APP_STACK    4

// Region Attributes for Cortex-M33
#define MPU_ATTR_READ_WRITE     (1UL << 0)
#define MPU_ATTR_READ_ONLY      (1UL << 1)
#define MPU_ATTR_EXECUTE_NEVER  (1UL << 2)
#define MPU_ATTR_PRIVILEGED     (1UL << 3)
#define MPU_ATTR_UNPRIVILEGED   (1UL << 4)

// Initialize the MPU
void mp_hal_mpu_init(void);

// Enable MPU protection for an Application thread
void mp_hal_mpu_enable_app_sandboxing(uint32_t code_start, uint32_t code_size, 
                                      uint32_t data_start, uint32_t data_size,
                                      uint32_t stack_start, uint32_t stack_size);

// Disable App sandboxing (Return to Kernel mode)
void mp_hal_mpu_disable_app_sandboxing(void);

#endif // MICROPOSIX_MPU_H
