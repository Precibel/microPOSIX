/**
 * @file context_switch.c
 * @brief Context Switch Implementation for RISC-V (nRF54L15)
 * 
 * This file implements the context switching for the RISC-V core on the
 * nRF54L15 SoC. It uses the machine-mode software interrupt for context switching.
 */

#include <stdint.h>
#include "microposix/kernel/thread.h"
#include "microposix/kernel/scheduler.h"
#include "microposix/hal/cpu.h"
#include "microposix/hal/nrf54l15/shared/ipc.h"

// =============================================================================
// RISC-V Register Definitions
// =============================================================================

// Machine-mode registers
#define MSTATUS      0x300  // Machine Status Register
#define MIE         0x304  // Machine Interrupt Enable Register
#define MIP         0x344  // Machine Interrupt Pending Register
#define MSCRATCH    0x340  // Machine Scratch Register
#define MEPC        0x341  // Machine Exception Program Counter
#define MCAUSE      0x342  // Machine Cause Register
#define MTVAL       0x343  // Machine Trap Value Register
#define MTVEC       0x305  // Machine Trap Vector Base Address Register

// Interrupt bits
#define MIP_MSIP    (1 << 3)  // Machine Software Interrupt Pending
#define MIE_MSIP    (1 << 3)  // Machine Software Interrupt Enable

// =============================================================================
// Stack Frame Definitions
// =============================================================================

/**
 * @brief Stack frame for RISC-V when an exception occurs.
 * 
 * For RISC-V, the following registers are saved:
 * - x1 (RA) - Return Address
 * - x3-x31 (General Purpose Registers)
 * - PC (Program Counter)
 * 
 * Note: x0 is zero and doesn't need to be saved.
 */
typedef struct {
    uint32_t ra;      // x1 - Return Address
    uint32_t gp[29];  // x3-x31 (29 registers)
    uint32_t pc;      // Program Counter
} riscv_stack_frame_t;

// =============================================================================
// Global Variables
// =============================================================================

// Current and next thread TCBs (defined in scheduler)
extern mp_tcb_t *current_thread;
extern mp_tcb_t *next_thread;

// =============================================================================
// Function Prototypes
// =============================================================================

// Assembly functions
void mp_hal_rv32_switch_to_thread(uint32_t sp);

// =============================================================================
// Context Switch Initialization
// =============================================================================

void mp_hal_cpu_init(void) {
    // Set up the machine-mode trap vector
    // For now, we'll just enable machine-mode software interrupts
    
    // Enable machine-mode software interrupts
    uint32_t mie;
    __asm volatile ("csrr %0, mie" : "=r" (mie));
    mie |= MIE_MSIP;
    __asm volatile ("csrw mie, %0" :: "r" (mie));
    
    // Set the trap vector base address
    // For now, we'll use a simple trap handler
    __asm volatile ("csrw mtvec, %0" :: "r" (&mp_hal_rv32_trap_handler));
}

// =============================================================================
// Stack Initialization
// =============================================================================

void mp_hal_cpu_init_stack(mp_tcb_t *tcb, mp_thread_func_t func, void *arg) {
    uint32_t *stack = (uint32_t *)tcb->stack_ptr;
    
    // Align stack to 16-byte boundary (recommended for RISC-V)
    stack = (uint32_t *)((uintptr_t)stack & ~0xF);
    
    // Initialize the stack frame
    // We need to set up the stack as if we were in the middle of a function call
    
    // Push the stack frame in reverse order
    // Start with the general purpose registers (x3-x31)
    for (int i = 0; i < 29; i++) {
        *(--stack) = 0;
    }
    
    // Set the return address (x1/RA) to the thread exit function
    *(--stack) = (uint32_t)mp_thread_exit;
    
    // Set the program counter (PC) to the thread function
    *(--stack) = (uint32_t)func;
    
    // Set the first argument (x10/A0)
    // Note: This is passed in a register, not on the stack
    // We'll set it up in the thread entry wrapper
    
    // Update the stack pointer in the TCB
    tcb->stack_ptr = (uintptr_t)stack;
}

// =============================================================================
// Context Switch Trigger
// =============================================================================

void mp_hal_cpu_trigger_context_switch(void) {
    // Set the machine software interrupt pending bit
    uint32_t mip;
    __asm volatile ("csrr %0, mip" : "=r" (mip));
    mip |= MIP_MSIP;
    __asm volatile ("csrw mip, %0" :: "r" (mip));
    
    // Ensure the write is visible
    __asm volatile ("fence");
}

// =============================================================================
// Critical Section Management
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

// =============================================================================
// Trap Handler
// =============================================================================

/**
 * @brief Machine-mode trap handler.
 * 
 * This handler is called for all machine-mode exceptions and interrupts.
 */
__attribute__((used)) void mp_hal_rv32_trap_handler(void) {
    uint32_t mcause;
    __asm volatile ("csrr %0, mcause" : "=r" (mcause));
    
    // Check if this is a machine software interrupt
    if ((mcause & 0x80000000) && ((mcause & 0x7FFFFFFF) == 3)) {
        // Machine software interrupt
        mp_hal_rv32_software_interrupt_handler();
    } else {
        // Other exception - handle as error for now
        while (1) {
            __asm volatile ("nop");
        }
    }
}

/**
 * @brief Machine software interrupt handler.
 * 
 * This handler performs the context switch.
 */
void mp_hal_rv32_software_interrupt_handler(void) {
    // Save the current context
    uint32_t sp;
    __asm volatile ("mv %0, sp" : "=r" (sp));
    
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
    mip &= ~MIP_MSIP;
    __asm volatile ("csrw mip, %0" :: "r" (mip));
    
    // Return from the interrupt
    __asm volatile ("mret");
}

// =============================================================================
// Thread Entry Wrapper
// =============================================================================

/**
 * @brief Thread entry wrapper for RISC-V.
 * 
 * This function is the entry point for all threads. It sets up the
 * argument register and calls the thread function.
 */
__attribute__((naked)) void mp_hal_rv32_thread_entry(void) {
    __asm volatile (
        // Load the argument from the stack
        "ld a0, 0(sp)\n"       // Load argument from stack
        
        // Call the thread function
        "ld t0, 4(sp)\n"       // Load function address
        "jalr ra, t0\n"       // Jump to function
        
        // Call thread exit
        "jal ra, mp_thread_exit\n"
        : : : "a0", "t0", "ra", "sp"
    );
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
// IPC Interrupt Enable for RV32
// =============================================================================

void mp_hal_nrf54l15_ipc_enable_interrupts(void) {
    // Enable machine-mode software interrupts
    uint32_t mie;
    __asm volatile ("csrr %0, mie" : "=r" (mie));
    mie |= MIE_MSIP;
    __asm volatile ("csrw mie, %0" :: "r" (mie));
    
    // Also enable the IPC peripheral interrupts
    NRF54L15_IPC_INTENSET = 0xFFFFFFFF;
}

// =============================================================================
// Utility Functions
// =============================================================================

uint32_t mp_hal_get_ticks(void) {
    // For RISC-V, we need a timer implementation
    // This is a placeholder - implement based on nRF54L15 timer peripheral
    
    // For now, return a dummy value
    static uint32_t tick_count = 0;
    return tick_count++;
}

void mp_hal_cpu_delay(uint32_t cycles) {
    // Simple delay loop
    volatile uint32_t i;
    for (i = 0; i < cycles; i++) {
        __asm volatile ("nop");
    }
}
