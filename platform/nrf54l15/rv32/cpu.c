/**
 * @file cpu.c
 * @brief CPU-Specific Functions for RISC-V (nRF54L15)
 * 
 * This file implements CPU-specific functions for the RISC-V core on the
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
    
    // 2. Initialize the trap vector
    mp_hal_rv32_trap_init();
    
    // 3. Initialize the machine-mode software interrupt
    mp_hal_rv32_software_interrupt_init();
}

// =============================================================================
// Trap Vector Initialization
// =============================================================================

void mp_hal_rv32_trap_init(void) {
    // Set the machine trap vector base address
    // We'll use a simple trap handler that jumps to our C handler
    __asm volatile ("csrw mtvec, %0" :: "r" (&mp_hal_rv32_trap_handler));
    
    // Enable machine-mode interrupts
    uint32_t mstatus;
    __asm volatile ("csrr %0, mstatus" : "=r" (mstatus));
    mstatus |= (1 << 3);  // Enable machine-mode interrupts (MIE)
    __asm volatile ("csrw mstatus, %0" :: "r" (mstatus));
}

// =============================================================================
// Software Interrupt Initialization
// =============================================================================

void mp_hal_rv32_software_interrupt_init(void) {
    // Enable machine-mode software interrupts
    uint32_t mie;
    __asm volatile ("csrr %0, mie" : "=r" (mie));
    mie |= (1 << 3);  // Enable machine software interrupt (MSIE)
    __asm volatile ("csrw mie, %0" :: "r" (mie));
}

// =============================================================================
// Core Identification
// =============================================================================

uint32_t mp_hal_nrf54l15_get_core_id(void) {
    // On nRF54L15, we can determine the core by reading a core-specific register
    // For RISC-V, we'll assume this is the RV32 core
    return CORE_ID_RV32;
}

// =============================================================================
// CPU-Specific Functions
// =============================================================================

uint32_t mp_hal_cpu_enter_critical(void) {
    uint32_t status;
    __asm volatile ("csrr %0, mstatus" : "=r" (status));
    __asm volatile ("csrci mstatus, 8");  // Disable machine interrupts (MIE)
    __asm volatile ("fence");
    return status;
}

void mp_hal_cpu_exit_critical(uint32_t status) {
    __asm volatile ("csrw mstatus, %0" :: "r" (status));
    __asm volatile ("fence");
}

void mp_hal_cpu_trigger_context_switch(void) {
    // Set the machine software interrupt pending bit
    uint32_t mip;
    __asm volatile ("csrr %0, mip" : "=r" (mip));
    mip |= (1 << 3);  // MIP_MSIP
    __asm volatile ("csrw mip, %0" :: "r" (mip));
    
    // Ensure the write is visible
    __asm volatile ("fence");
}

// =============================================================================
// Startup Code
// =============================================================================

/**
 * @brief Early initialization for the RV32 core.
 * 
 * This function is called very early in the boot process, before main().
 */
void mp_hal_rv32_early_init(void) {
    // 1. Set up the stack pointer
    // (This is typically done by the startup assembly)
    
    // 2. Initialize the trap vector
    mp_hal_rv32_trap_init();
    
    // 3. Initialize the IPC system
    mp_hal_nrf54l15_ipc_init();
    
    // 4. Signal that this core is ready
    mp_hal_nrf54l15_signal_core_ready();
}

/**
 * @brief Main initialization for the RV32 core.
 * 
 * This function is called after early initialization, before the scheduler starts.
 */
void mp_hal_rv32_main_init(void) {
    // 1. Initialize the CPU
    mp_hal_cpu_init();
    
    // 2. Initialize the software interrupt
    mp_hal_rv32_software_interrupt_init();
    
    // 3. Initialize core synchronization
    mp_hal_nrf54l15_core_sync_init();
}
