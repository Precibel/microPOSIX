/**
 * @file irq_handlers.c
 * @brief Interrupt Request Handlers for RISC-V (nRF54L15)
 * 
 * This file contains additional interrupt handlers for the RISC-V core on the
 * nRF54L15 SoC.
 */

#include <stdint.h>
#include "microposix/hal/nrf54l15/shared/ipc.h"

// =============================================================================
// IPC Interrupt Handler
// =============================================================================

/**
 * @brief IPC interrupt handler for RISC-V.
 * 
 * This handler is called when the M33 core sends an IPC message.
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

// =============================================================================
// Timer Interrupt Handler
// =============================================================================

/**
 * @brief Timer interrupt handler for RISC-V.
 * 
 * This handler is called when a timer interrupt occurs.
 */
__attribute__((used)) void Timer_Handler(void) {
    // Clear the timer interrupt
    // (Implementation depends on the specific timer peripheral)
    
    // Call the scheduler tick function
    // mp_scheduler_tick();
    
    // Trigger a context switch if needed
    // mp_hal_cpu_trigger_context_switch();
}

// =============================================================================
// External Interrupt Handlers
// =============================================================================

/**
 * @brief External interrupt 0 handler.
 */
__attribute__((used)) void External_Interrupt_0_Handler(void) {
    // Handle external interrupt 0
}

/**
 * @brief External interrupt 1 handler.
 */
__attribute__((used)) void External_Interrupt_1_Handler(void) {
    // Handle external interrupt 1
}

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * @brief Enable interrupts globally.
 */
void mp_hal_rv32_enable_interrupts(void) {
    uint32_t mstatus;
    __asm volatile ("csrr %0, mstatus" : "=r" (mstatus));
    mstatus |= (1 << 3);  // Enable machine-mode interrupts (MIE)
    __asm volatile ("csrw mstatus, %0" :: "r" (mstatus));
}

/**
 * @brief Disable interrupts globally.
 */
void mp_hal_rv32_disable_interrupts(void) {
    uint32_t mstatus;
    __asm volatile ("csrr %0, mstatus" : "=r" (mstatus));
    mstatus &= ~(1 << 3);  // Disable machine-mode interrupts (MIE)
    __asm volatile ("csrw mstatus, %0" :: "r" (mstatus));
}

/**
 * @brief Get the current interrupt status.
 * 
 * @return The current mstatus register value.
 */
uint32_t mp_hal_rv32_get_interrupt_status(void) {
    uint32_t mstatus;
    __asm volatile ("csrr %0, mstatus" : "=r" (mstatus));
    return mstatus;
}

/**
 * @brief Set the interrupt status.
 * 
 * @param status The mstatus register value to set.
 */
void mp_hal_rv32_set_interrupt_status(uint32_t status) {
    __asm volatile ("csrw mstatus, %0" :: "r" (status));
}
