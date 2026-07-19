/**
 * @file startup.c
 * @brief Startup Code for Cortex-M33 (nRF54L15)
 * 
 * This file contains the early startup code for the Cortex-M33 core on the
 * nRF54L15 SoC. It initializes the hardware and prepares for the main function.
 */

#include <stdint.h>
#include "microposix/hal/nrf54l15/shared/ipc.h"
#include "microposix/hal/nrf54l15/shared/shared_memory.h"

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
void mp_hal_m33_early_init(void);
void mp_hal_m33_main_init(void);

// =============================================================================
// Reset Handler
// =============================================================================

/**
 * @brief Reset handler - first code executed after reset.
 * 
 * This function:
 * 1. Sets up the stack pointer
 * 2. Copies initialized data from flash to RAM
 * 3. Zero-initializes the BSS section
 * 4. Calls early initialization
 * 5. Calls main()
 */
__attribute__((section(".reset_handler"))) void Reset_Handler(void) {
    // Set stack pointer
    __asm volatile (
        "ldr sp, =__stack_top\n"
        "msr msp, sp\n"
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
    mp_hal_m33_early_init();
    
    // Call main initialization
    mp_hal_m33_main_init();
    
    // Call main()
    main();
    
    // Should never reach here
    while (1) {
        __asm volatile ("wfi");  // Wait for interrupt
    }
}

// =============================================================================
// Default Exception Handlers
// =============================================================================

/**
 * @brief Default exception handler.
 * 
 * This handler is used for all exceptions that don't have a specific handler.
 */
__attribute__((section(".exception_handlers"))) void Default_Handler(void) {
    // Infinite loop
    while (1) {
        __asm volatile ("nop");
    }
}

// =============================================================================
// Exception Vector Table
// =============================================================================

/**
 * @brief Exception vector table.
 * 
 * This table contains the addresses of all exception handlers.
 * It must be aligned to a 512-byte boundary.
 */
__attribute__((section(".vectors"), aligned(512))) const uint32_t vector_table[] = {
    // Stack top
    (uint32_t)&__stack_top,
    
    // Exception handlers
    (uint32_t)Reset_Handler,           // 1: Reset
    (uint32_t)Default_Handler,        // 2: NMI
    (uint32_t)Default_Handler,        // 3: HardFault
    (uint32_t)Default_Handler,        // 4: MemManage
    (uint32_t)Default_Handler,        // 5: BusFault
    (uint32_t)Default_Handler,        // 6: UsageFault
    (uint32_t)Default_Handler,        // 7: Reserved
    (uint32_t)Default_Handler,        // 8: Reserved
    (uint32_t)Default_Handler,        // 9: Reserved
    (uint32_t)Default_Handler,        // 10: Reserved
    (uint32_t)Default_Handler,        // 11: SVCall
    (uint32_t)Default_Handler,        // 12: DebugMonitor
    (uint32_t)Default_Handler,        // 13: Reserved
    (uint32_t)Default_Handler,        // 14: PendSV
    (uint32_t)Default_Handler,        // 15: SysTick
    
    // Peripheral interrupts (nRF54L15-specific)
    // These will be filled in by the linker or startup code
    (uint32_t)Default_Handler,        // 16: External Interrupt 0
    (uint32_t)Default_Handler,        // 17: External Interrupt 1
    // ... more interrupts as needed
};

// =============================================================================
// Weak Aliases for Exception Handlers
// =============================================================================

// These weak aliases allow the application to override specific handlers
__attribute__((weak, alias("Default_Handler"))) void NMI_Handler(void);
__attribute__((weak, alias("Default_Handler"))) void HardFault_Handler(void);
__attribute__((weak, alias("Default_Handler"))) void MemManage_Handler(void);
__attribute__((weak, alias("Default_Handler"))) void BusFault_Handler(void);
__attribute__((weak, alias("Default_Handler"))) void UsageFault_Handler(void);
__attribute__((weak, alias("Default_Handler"))) void SVC_Handler(void);
__attribute__((weak, alias("Default_Handler"))) void DebugMon_Handler(void);
__attribute__((weak, alias("Default_Handler"))) void PendSV_Handler(void);
__attribute__((weak, alias("Default_Handler"))) void SysTick_Handler(void);

// =============================================================================
// PendSV Handler (Context Switch)
// =============================================================================

/**
 * @brief PendSV handler for context switching.
 * 
 * This handler saves the context of the current thread and restores the
 * context of the next thread.
 */
__attribute__((used)) void PendSV_Handler(void) {
    // This is a placeholder. The actual implementation should be in assembly
    // for better performance and to handle the stack properly.
    
    // Call the microPOSIX PendSV handler
    mp_hal_cpu_pendsv_handler();
}

// =============================================================================
// SysTick Handler
// =============================================================================

/**
 * @brief SysTick handler for scheduler tick.
 */
__attribute__((used)) void SysTick_Handler(void) {
    // Call the microPOSIX SysTick handler
    mp_hal_cpu_systick_handler();
}

// =============================================================================
// IPC Interrupt Handler
// =============================================================================

/**
 * @brief IPC interrupt handler.
 * 
 * This handler is called when the other core sends an IPC message.
 */
__attribute__((used)) void IPC_Handler(void) {
    // Acknowledge the interrupt
    NRF54L15_IPC_INTSTATUS = 0xFFFFFFFF;  // Clear all pending interrupts
    
    // Process the IPC message
    ipc_message_t msg;
    if (mp_hal_nrf54l15_ipc_receive_nonblocking(&msg) == 0) {
        // Handle the message based on its type
        switch (msg.type) {
            case IPC_MSG_TYPE_CORE_READY:
                // The other core is ready
                mp_hal_nrf54l15_signal_core_ready();
                break;
                
            case IPC_MSG_TYPE_THREAD_CREATE:
                // Request to create a thread on this core
                // (Implementation depends on your thread management)
                break;
                
            case IPC_MSG_TYPE_MUTEX_LOCK:
                // Request to lock a cross-core mutex
                break;
                
            default:
                // Unknown message type
                break;
        }
    }
}
