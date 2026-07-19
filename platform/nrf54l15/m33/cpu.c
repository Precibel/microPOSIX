/**
 * @file cpu.c
 * @brief CPU-Specific Functions for Cortex-M33 (nRF54L15)
 * 
 * This file implements CPU-specific functions for the Cortex-M33 core on the
 * nRF54L15 SoC.
 */

#include <stdint.h>
#include "microposix/hal/cpu.h"
#include "microposix/hal/nrf54l15/shared/ipc.h"
#include "microposix/kernel/scheduler.h"

// =============================================================================
// CPU Initialization
// =============================================================================

void mp_hal_cpu_init(void) {
    // Initialize the CPU for microPOSIX
    
    // 1. Set up the stack for the main thread
    // (This is typically done by the startup code)
    
    // 2. Initialize the FPU
    mp_hal_m33_fpu_init();
    
    // 3. Initialize the MPU (if available)
    mp_hal_m33_mpu_init();
    
    // 4. Initialize the SysTick timer
    mp_hal_m33_systick_init();
    
    // 5. Initialize the PendSV interrupt
    mp_hal_m33_pendsv_init();
}

// =============================================================================
// FPU Initialization
// =============================================================================

void mp_hal_m33_fpu_init(void) {
    // Enable the FPU (Floating Point Unit)
    // nRF54L15 Cortex-M33 has a single-precision FPU
    
    // Enable CP10 and CP11 (FPU) in the CPACR register
    uint32_t cpacr = *((volatile uint32_t *)0xE000ED88);  // SCB_CPACR
    cpacr |= (0xFUL << 20);  // Enable CP10 and CP11
    *((volatile uint32_t *)0xE000ED88) = cpacr;
    
    // Ensure the write is complete
    __asm volatile ("dsb");
    __asm volatile ("isb");
    
    // Enable FPU in the CONTROL register
    uint32_t control;
    __asm volatile ("mrs %0, control" : "=r" (control));
    control |= (1 << 2);  // Enable FPU
    __asm volatile ("msr control, %0" :: "r" (control));
    
    // Ensure the write is complete
    __asm volatile ("dsb");
    __asm volatile ("isb");
}

// =============================================================================
// MPU Initialization
// =============================================================================

void mp_hal_m33_mpu_init(void) {
    // Initialize the Memory Protection Unit
    // This is a basic initialization. For production, configure regions as needed.
    
    // Disable the MPU
    uint32_t mpu_ctrl = *((volatile uint32_t *)0xE000ED94);  // MPU_CTRL
    mpu_ctrl &= ~(1 << 0);  // Disable MPU
    *((volatile uint32_t *)0xE000ED94) = mpu_ctrl;
    
    // Invalidate all MPU regions
    for (int i = 0; i < 16; i++) {
        *((volatile uint32_t *)0xE000ED98) = i;  // MPU_RNR
        *((volatile uint32_t *)0xE000EDA4) = 0;  // MPU_RLAR (disable region)
    }
    
    // Configure default memory map (disable MPU for now)
    // For production, configure regions for code, data, stack, etc.
    
    // Enable the MPU
    mpu_ctrl |= (1 << 0);  // Enable MPU
    *((volatile uint32_t *)0xE000ED94) = mpu_ctrl;
    
    // Ensure the write is complete
    __asm volatile ("dsb");
    __asm volatile ("isb");
}

// =============================================================================
// SysTick Initialization
// =============================================================================

void mp_hal_m33_systick_init(void) {
    // Configure SysTick for the scheduler tick
    // Assuming a 1ms tick at 96MHz (nRF54L15 typical clock)
    
    volatile uint32_t *systick_load = (volatile uint32_t *)0xE000E014;
    volatile uint32_t *systick_val = (volatile uint32_t *)0xE000E018;
    volatile uint32_t *systick_ctrl = (volatile uint32_t *)0xE000E010;
    
    // Calculate reload value for 1ms at 96MHz
    // SysTick clock is typically CPU clock / 8 for Cortex-M
    // For nRF54L15, check the actual clock configuration
    uint32_t cpu_clock = 96000000;  // 96 MHz
    uint32_t systick_clock = cpu_clock / 8;  // SysTick clock is CPU clock / 8
    uint32_t reload_value = systick_clock / 1000 - 1;  // 1ms tick
    
    // Set reload value
    *systick_load = reload_value;
    
    // Clear current value
    *systick_val = 0;
    
    // Enable SysTick with CPU clock source and interrupt
    *systick_ctrl = (1 << 0) |  // Enable
                   (1 << 1) |  // Tick interrupt
                   (1 << 2);   // CPU clock source
    
    // Ensure the write is complete
    __asm volatile ("dsb");
    __asm volatile ("isb");
}

// =============================================================================
// PendSV Initialization
// =============================================================================

void mp_hal_m33_pendsv_init(void) {
    // Set PendSV priority to the lowest
    // On Cortex-M, the priority is in the upper 8 bits of the SHPR3 register
    volatile uint32_t *shpr3 = (volatile uint32_t *)0xE000ED20;
    *shpr3 = (*shpr3 & ~(0xFFUL << 24)) | (0xFFUL << 24);  // Priority 255 (lowest)
    
    // Ensure the write is complete
    __asm volatile ("dsb");
    __asm volatile ("isb");
}

// =============================================================================
// Core Identification
// =============================================================================

uint32_t mp_hal_nrf54l15_get_core_id(void) {
    // On nRF54L15, we can determine the core by reading a core-specific register
    // For Cortex-M33, we can check the CPUID register
    
    uint32_t cpuid = *((volatile uint32_t *)0xE000ED00);
    
    // Check if this is a Cortex-M33
    // CPUID[4:0] = 0xD (Cortex-M33)
    if ((cpuid & 0xFF) == 0xD) {
        return CORE_ID_M33;
    }
    
    // Default to M33 for safety
    return CORE_ID_M33;
}

// =============================================================================
// CPU-Specific Functions
// =============================================================================

uint32_t mp_hal_cpu_enter_critical(void) {
    uint32_t status;
    __asm volatile ("mrs %0, primask" : "=r" (status));
    __asm volatile ("cpsid i");  // Disable interrupts
    __asm volatile ("dsb");
    __asm volatile ("isb");
    return status;
}

void mp_hal_cpu_exit_critical(uint32_t status) {
    __asm volatile ("msr primask, %0" :: "r" (status));
    __asm volatile ("dsb");
    __asm volatile ("isb");
}

void mp_hal_cpu_trigger_context_switch(void) {
    // Set the PendSV pending bit
    volatile uint32_t *icsr = (volatile uint32_t *)0xE000ED04;
    *icsr |= (1 << 28);  // PENDSVSET
    
    // Ensure the write is visible to the CPU
    __asm volatile ("dsb");
    __asm volatile ("isb");
}

// =============================================================================
// Startup Code
// =============================================================================

/**
 * @brief Early initialization for the M33 core.
 * 
 * This function is called very early in the boot process, before main().
 */
void mp_hal_m33_early_init(void) {
    // 1. Set up the stack pointer
    // (This is typically done by the startup assembly)
    
    // 2. Initialize the FPU
    mp_hal_m33_fpu_init();
    
    // 3. Initialize the MPU (optional)
    // mp_hal_m33_mpu_init();
    
    // 4. Initialize the IPC system
    mp_hal_nrf54l15_ipc_init();
    
    // 5. Initialize shared memory
    mp_hal_nrf54l15_shared_memory_init();
    
    // 6. Start the other core (RISC-V)
    mp_hal_nrf54l15_start_other_core();
    
    // 7. Wait for the other core to be ready
    mp_hal_nrf54l15_wait_for_other_core_ready(1000);  // 1 second timeout
    
    // 8. Signal that this core is ready
    mp_hal_nrf54l15_signal_core_ready();
}

/**
 * @brief Main initialization for the M33 core.
 * 
 * This function is called after early initialization, before the scheduler starts.
 */
void mp_hal_m33_main_init(void) {
    // 1. Initialize the CPU
    mp_hal_cpu_init();
    
    // 2. Initialize the SysTick timer
    mp_hal_m33_systick_init();
    
    // 3. Initialize the PendSV interrupt
    mp_hal_m33_pendsv_init();
    
    // 4. Initialize core synchronization
    mp_hal_nrf54l15_core_sync_init();
}
