#include <stdint.h>
#include <stdbool.h>
#include "microposix/hal/mpu.h"

// Cortex-M33 MPU Registers (SCS: 0xE000ED00)
#define MPU_TYPE      (*((volatile uint32_t *)0xE000ED90))
#define MPU_CTRL      (*((volatile uint32_t *)0xE000ED94))
#define MPU_RNR       (*((volatile uint32_t *)0xE000ED98))
#define MPU_RBAR      (*((volatile uint32_t *)0xE000ED9C))
#define MPU_RLAR      (*((volatile uint32_t *)0xE000EDA0))

// MPU Control Bits
#define MPU_CTRL_ENABLE      (1UL << 0)
#define MPU_CTRL_PRIVDEFENA  (1UL << 2) // Privileged Default Memory Map Enable

void mp_hal_mpu_init(void) {
    // Basic MPU setup: enable default background map for privileged mode
    MPU_CTRL = MPU_CTRL_PRIVDEFENA | MPU_CTRL_ENABLE;
}

void mp_hal_mpu_enable_app_sandboxing(uint32_t code_start, uint32_t code_size, 
                                      uint32_t data_start, uint32_t data_size,
                                      uint32_t stack_start, uint32_t stack_size) {
    // 1. Configure App Code Region (Read/Execute, Unprivileged)
    MPU_RNR  = MPU_REGION_APP_CODE;
    MPU_RBAR = (code_start & 0xFFFFFFE0); // 32-byte alignment
    MPU_RLAR = ((code_start + code_size - 1) & 0xFFFFFFE0) | (1UL << 0); // Enabled
    
    // 2. Configure App Data Region (Read/Write, Execute-Never, Unprivileged)
    MPU_RNR  = MPU_REGION_APP_DATA;
    MPU_RBAR = (data_start & 0xFFFFFFE0);
    MPU_RLAR = ((data_start + data_size - 1) & 0xFFFFFFE0) | (1UL << 0);
    
    // 3. Configure App Stack Region (Read/Write, Execute-Never, Unprivileged)
    MPU_RNR  = MPU_REGION_APP_STACK;
    MPU_RBAR = (stack_start & 0xFFFFFFE0);
    MPU_RLAR = ((stack_start + stack_size - 1) & 0xFFFFFFE0) | (1UL << 0);
}

void mp_hal_mpu_disable_app_sandboxing(void) {
    // Disable regions used for application
    MPU_RNR  = MPU_REGION_APP_CODE;
    MPU_RLAR = 0;
    MPU_RNR  = MPU_REGION_APP_DATA;
    MPU_RLAR = 0;
    MPU_RNR  = MPU_REGION_APP_STACK;
    MPU_RLAR = 0;
}
