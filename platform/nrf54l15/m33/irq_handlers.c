/**
 * @file irq_handlers.c
 * @brief Interrupt Request Handlers for Cortex-M33 (nRF54L15)
 * 
 * This file contains the interrupt handlers for the Cortex-M33 core on the
 * nRF54L15 SoC. It includes the PendSV and SysTick handlers in C for
 * cases where assembly is not available.
 */

#include <stdint.h>
#include "microposix/kernel/thread.h"
#include "microposix/kernel/scheduler.h"
#include "microposix/hal/cpu.h"

// =============================================================================
// Global Variables
// =============================================================================

// Current and next thread TCBs (defined in scheduler)
extern mp_tcb_t *current_thread;
extern mp_tcb_t *next_thread;

// =============================================================================
// PendSV Handler (C Implementation)
// =============================================================================

/**
 * @brief PendSV handler implementation in C.
 * 
 * This handler saves the context of the current thread and restores the
 * context of the next thread. It's called from the PendSV interrupt.
 * 
 * Note: For better performance, this should be implemented in assembly.
 * This C implementation is provided as a fallback.
 */
void mp_hal_cpu_pendsv_handler(void) {
    // Save the context of the current thread
    if (current_thread != NULL) {
        // Get the current stack pointer
        uint32_t sp;
        __asm volatile ("mrs %0, msp" : "=r" (sp));
        
        // Save the stack pointer in the TCB
        current_thread->stack_ptr = sp;
    }
    
    // Switch to the next thread
    if (next_thread != NULL) {
        current_thread = next_thread;
        next_thread = NULL;
        
        // Restore the stack pointer from the TCB
        uint32_t sp = current_thread->stack_ptr;
        __asm volatile ("msr msp, %0" :: "r" (sp));
    }
    
    // Return from the PendSV interrupt
    // The context will be restored automatically by the CPU
}

// =============================================================================
// SysTick Handler
// =============================================================================

/**
 * @brief SysTick handler for scheduler tick.
 */
void mp_hal_cpu_systick_handler(void) {
    // Call the scheduler tick function
    mp_scheduler_tick();
    
    // Trigger a context switch if needed
    mp_hal_cpu_trigger_context_switch();
}

// =============================================================================
// Context Switch Assembly Implementation
// =============================================================================

// If you prefer to use assembly for better performance, you can replace the
// C implementation above with assembly. Here's an example of what the
// assembly implementation might look like:

__attribute__((section(".text.pendsv"))) 
__attribute__((naked)) 
void mp_hal_m33_pendsv_handler_asm(void) {
    __asm volatile (
        // Save the current context
        "push {r4-r11}\n"          // Save callee-saved registers
        "mrs r0, psp\n"           // Get the process stack pointer
        "stm r0!, {r4-r11}\n"      // Save registers to the stack
        
        // Save the stack pointer in the current thread's TCB
        "ldr r1, =current_thread\n"
        "ldr r1, [r1]\n"           // Load current_thread pointer
        "str r0, [r1, #0]\n"        // Save stack_ptr in TCB
        
        // Switch to the next thread
        "ldr r1, =next_thread\n"
        "ldr r2, [r1]\n"           // Load next_thread pointer
        "cmp r2, #0\n"             // Check if next_thread is NULL
        "beq 1f\n"                // If NULL, skip
        
        // Set current_thread = next_thread
        "ldr r3, =current_thread\n"
        "str r2, [r3]\n"           // Store next_thread in current_thread
        
        // Set next_thread = NULL
        "mov r2, #0\n"
        "str r2, [r1]\n"           // Store NULL in next_thread
        
        // Restore the stack pointer from the TCB
        "ldr r0, [r2, #0]\n"        // Load stack_ptr from TCB
        
        // Restore the context
        "ldm r0!, {r4-r11}\n"      // Restore registers from the stack
        "msr psp, r0\n"           // Set the process stack pointer
        
        "1:\n"
        // Return from the PendSV interrupt
        "bx lr\n"
        : : : "r0", "r1", "r2", "r3", "memory"
    );
}

// =============================================================================
// FPU Lazy Stacking Support
// =============================================================================

/**
 * @brief Enable FPU lazy stacking.
 * 
 * This function enables the FPU to use lazy stacking, which means that
 * the FPU registers are only saved when an exception occurs and the FPU
 * was in use.
 */
void mp_hal_m33_fpu_enable_lazy_stacking(void) {
    // Set the FPCCR (Floating-Point Context Control Register)
    // to enable lazy stacking
    uint32_t fpccr = *((volatile uint32_t *)0xE000EF34);
    fpccr |= (1 << 30);  // ASPEN (Automatic State Preservation Enable)
    fpccr |= (1 << 31);  // LSPEN (Lazy State Preservation Enable)
    *((volatile uint32_t *)0xE000EF34) = fpccr;
    
    // Ensure the write is complete
    __asm volatile ("dsb");
    __asm volatile ("isb");
}

// =============================================================================
// MPU Region Configuration
// =============================================================================

/**
 * @brief Configure an MPU region.
 * 
 * @param region_num The MPU region number (0-15).
 * @param base_addr The base address of the region.
 * @param size The size of the region (must be a power of 2).
 * @param permissions The permissions for the region (e.g., 0x03 for R/W).
 */
void mp_hal_m33_mpu_configure_region(uint32_t region_num, uint32_t base_addr, 
                                     uint32_t size, uint32_t permissions) {
    volatile uint32_t *mpu_rnr = (volatile uint32_t *)0xE000ED98;  // MPU Region Number Register
    volatile uint32_t *mpu_rbar = (volatile uint32_t *)0xE000ED9C; // MPU Region Base Address Register
    volatile uint32_t *mpu_rlar = (volatile uint32_t *)0xE000EDA0; // MPU Region Limit Address Register
    
    // Select the region
    *mpu_rnr = region_num;
    
    // Configure the base address
    *mpu_rbar = base_addr | (1 << 0);  // VALID bit
    
    // Configure the limit address and permissions
    // The size is encoded in the RLAR register
    uint32_t size_encoding = 0;
    if (size >= (1 << 5)) { size_encoding = 0x02; }  // 32 bytes
    if (size >= (1 << 6)) { size_encoding = 0x03; }  // 64 bytes
    if (size >= (1 << 7)) { size_encoding = 0x04; }  // 128 bytes
    if (size >= (1 << 8)) { size_encoding = 0x05; }  // 256 bytes
    if (size >= (1 << 9)) { size_encoding = 0x06; }  // 512 bytes
    if (size >= (1 << 10)) { size_encoding = 0x07; } // 1KB
    if (size >= (1 << 11)) { size_encoding = 0x08; } // 2KB
    if (size >= (1 << 12)) { size_encoding = 0x09; } // 4KB
    if (size >= (1 << 13)) { size_encoding = 0x0A; } // 8KB
    if (size >= (1 << 14)) { size_encoding = 0x0B; } // 16KB
    if (size >= (1 << 15)) { size_encoding = 0x0C; } // 32KB
    if (size >= (1 << 16)) { size_encoding = 0x0D; } // 64KB
    if (size >= (1 << 17)) { size_encoding = 0x0E; } // 128KB
    if (size >= (1 << 18)) { size_encoding = 0x0F; } // 256KB
    if (size >= (1 << 19)) { size_encoding = 0x10; } // 512KB
    if (size >= (1 << 20)) { size_encoding = 0x11; } // 1MB
    if (size >= (1 << 21)) { size_encoding = 0x12; } // 2MB
    if (size >= (1 << 22)) { size_encoding = 0x13; } // 4MB
    if (size >= (1 << 23)) { size_encoding = 0x14; } // 8MB
    if (size >= (1 << 24)) { size_encoding = 0x15; } // 16MB
    if (size >= (1 << 25)) { size_encoding = 0x16; } // 32MB
    if (size >= (1 << 26)) { size_encoding = 0x17; } // 64MB
    if (size >= (1 << 27)) { size_encoding = 0x18; } // 128MB
    if (size >= (1 << 28)) { size_encoding = 0x19; } // 256MB
    if (size >= (1 << 29)) { size_encoding = 0x1A; } // 512MB
    if (size >= (1 << 30)) { size_encoding = 0x1B; } // 1GB
    if (size >= (1 << 31)) { size_encoding = 0x1C; } // 2GB
    
    *mpu_rlar = (base_addr + size - 1) | (permissions << 24) | (size_encoding << 1) | (1 << 0);
    
    // Ensure the write is complete
    __asm volatile ("dsb");
    __asm volatile ("isb");
}
