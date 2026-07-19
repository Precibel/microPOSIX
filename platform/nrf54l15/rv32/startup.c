/**
 * @file startup.c
 * @brief Startup Code for RISC-V (nRF54L15)
 * 
 * This file contains the early startup code for the RISC-V core on the
 * nRF54L15 SoC. It initializes the hardware and prepares for the main function.
 */

#include <stdint.h>
#include "microposix/hal/nrf54l15/shared/ipc.h"

// =============================================================================
// External Symbols
// =============================================================================

// Stack top (defined in linker script)
extern uint32_t __stack_top;

// Data section boundaries
extern uint32_t __data_start__;
extern uint32_t __data_end__;
extern uint32_t __data_load__;

// BSS section boundaries
extern uint32_t __bss_start__;
extern uint32_t __bss_end__;

// =============================================================================
// Function Prototypes
// =============================================================================

// Main function
int main(void);

// CPU initialization
void mp_hal_rv32_early_init(void);
void mp_hal_rv32_main_init(void);

// =============================================================================
// Reset Handler (Entry Point)
// =============================================================================

/**
 * @brief Entry point for the RISC-V core.
 * 
 * This function is the first code executed when the RISC-V core starts.
 * It performs the following:
 * 1. Sets up the stack pointer
 * 2. Copies initialized data from flash to RAM
 * 3. Zero-initializes the BSS section
 * 4. Calls early initialization
 * 5. Calls main()
 */
__attribute__((section(".text.startup"))) void _start(void) {
    // Set stack pointer
    __asm volatile (
        "la sp, __stack_top\n"
        : : : "sp"
    );
    
    // Copy initialized data from flash to RAM
    uint32_t *data_src = &__data_load__;
    uint32_t *data_dst = &__data_start__;
    
    while (data_dst < &__data_end__) {
        *data_dst++ = *data_src++;
    }
    
    // Zero-initialize the BSS section
    uint32_t *bss_dst = &__bss_start__;
    
    while (bss_dst < &__bss_end__) {
        *bss_dst++ = 0;
    }
    
    // Call early initialization
    mp_hal_rv32_early_init();
    
    // Call main initialization
    mp_hal_rv32_main_init();
    
    // Call main()
    main();
    
    // Should never reach here
    while (1) {
        __asm volatile ("wfi");  // Wait for interrupt (if supported)
    }
}

// =============================================================================
// Trap Vector
// =============================================================================

/**
 * @brief Machine-mode trap vector.
 * 
 * This is the entry point for all machine-mode exceptions and interrupts.
 * It saves the context and jumps to the trap handler.
 */
__attribute__((section(".text.trap_vector"), aligned(256))) void mp_hal_rv32_trap_vector(void) {
    // Save the context
    // For simplicity, we'll just jump to the handler
    // A proper implementation would save all registers
    
    __asm volatile (
        // Save all general-purpose registers
        "addi sp, sp, -128\n"
        "sd ra, 0(sp)\n"
        "sd sp, 8(sp)\n"
        "sd gp, 16(sp)\n"
        "sd tp, 24(sp)\n"
        "sd t0, 32(sp)\n"
        "sd t1, 40(sp)\n"
        "sd t2, 48(sp)\n"
        "sd s0, 56(sp)\n"
        "sd s1, 64(sp)\n"
        "sd a0, 72(sp)\n"
        "sd a1, 80(sp)\n"
        "sd a2, 88(sp)\n"
        "sd a3, 96(sp)\n"
        "sd a4, 104(sp)\n"
        "sd a5, 112(sp)\n"
        "sd a6, 120(sp)\n"
        "sd a7, 128(sp)\n"
        
        // Call the trap handler
        "call mp_hal_rv32_trap_handler\n"
        
        // Restore the context
        "ld ra, 0(sp)\n"
        "ld sp, 8(sp)\n"
        "ld gp, 16(sp)\n"
        "ld tp, 24(sp)\n"
        "ld t0, 32(sp)\n"
        "ld t1, 40(sp)\n"
        "ld t2, 48(sp)\n"
        "ld s0, 56(sp)\n"
        "ld s1, 64(sp)\n"
        "ld a0, 72(sp)\n"
        "ld a1, 80(sp)\n"
        "ld a2, 88(sp)\n"
        "ld a3, 96(sp)\n"
        "ld a4, 104(sp)\n"
        "ld a5, 112(sp)\n"
        "ld a6, 120(sp)\n"
        "ld a7, 128(sp)\n"
        "addi sp, sp, 128\n"
        
        // Return from trap
        "mret\n"
        : : : "memory"
    );
}

// =============================================================================
// Default Trap Handler
// =============================================================================

/**
 * @brief Default trap handler.
 * 
 * This handler is called for all traps that don't have a specific handler.
 */
__attribute__((section(".text.trap_handlers"))) void mp_hal_rv32_trap_handler(void) {
    uint32_t mcause;
    __asm volatile ("csrr %0, mcause" : "=r" (mcause));
    
    uint32_t mepc;
    __asm volatile ("csrr %0, mepc" : "=r" (mepc));
    
    // Check the cause of the trap
    if (mcause & 0x80000000) {
        // Interrupt
        uint32_t irq = mcause & 0x7FFFFFFF;
        
        switch (irq) {
            case 3: // Machine Software Interrupt
                mp_hal_rv32_software_interrupt_handler();
                break;
            default:
                // Unknown interrupt
                while (1) {
                    __asm volatile ("nop");
                }
        }
    } else {
        // Exception
        switch (mcause) {
            case 0: // Instruction address misaligned
            case 1: // Instruction access fault
            case 2: // Illegal instruction
            case 3: // Breakpoint
            case 4: // Load address misaligned
            case 5: // Load access fault
            case 6: // Store/AMO address misaligned
            case 7: // Store/AMO access fault
            case 8: // Environment call from U-mode
            case 9: // Environment call from S-mode
            case 11: // Environment call from M-mode
            default:
                // Unknown exception
                while (1) {
                    __asm volatile ("nop");
                }
        }
    }
}

// =============================================================================
// Software Interrupt Handler
// =============================================================================

/**
 * @brief Machine software interrupt handler.
 * 
 * This handler performs the context switch when triggered.
 */
__attribute__((section(".text.trap_handlers"))) void mp_hal_rv32_software_interrupt_handler(void) {
    // Save the current context
    uint32_t sp;
    __asm volatile ("mv %0, sp" : "=r" (sp));
    
    extern mp_tcb_t *current_thread;
    extern mp_tcb_t *next_thread;
    
    if (current_thread != NULL) {
        // Save the stack pointer
        current_thread->stack_ptr = sp;
    }
    
    // Switch to the next thread
    if (next_thread != NULL) {
        current_thread = next_thread;
        next_thread = NULL;
        
        // Restore the stack pointer
        sp = current_thread->stack_ptr;
        __asm volatile ("mv sp, %0" :: "r" (sp));
    }
    
    // Clear the software interrupt pending bit
    uint32_t mip;
    __asm volatile ("csrr %0, mip" : "=r" (mip));
    mip &= ~(1 << 3);  // Clear MIP_MSIP
    __asm volatile ("csrw mip, %0" :: "r" (mip));
    
    // Return from the interrupt
    __asm volatile ("mret");
}
