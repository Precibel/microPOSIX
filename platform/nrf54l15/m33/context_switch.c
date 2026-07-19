/**
 * @file context_switch.c
 * @brief Context Switch Implementation for Cortex-M33 (nRF54L15)
 * 
 * This file implements the context switching for the Cortex-M33 core on the
 * nRF54L15 SoC. It uses the PendSV interrupt for context switching.
 */

#include <stdint.h>
#include "microposix/kernel/thread.h"
#include "microposix/kernel/scheduler.h"
#include "microposix/hal/cpu.h"
#include "microposix/hal/nrf54l15/shared/ipc.h"

// =============================================================================
// Cortex-M33 Register Definitions
// =============================================================================

// System Control Block (SCB) registers
#define SCB_CPUID       (*((volatile uint32_t *)0xE000ED00))
#define SCB_ICSR        (*((volatile uint32_t *)0xE000ED04))
#define SCB_VTOR        (*((volatile uint32_t *)0xE000ED08))
#define SCB_AIRCR       (*((volatile uint32_t *)0xE000ED0C))
#define SCB_SCR         (*((volatile uint32_t *)0xE000ED10))
#define SCB_CCR         (*((volatile uint32_t *)0xE000ED14))
#define SCB_SHPR2       (*((volatile uint32_t *)0xE000ED1C))
#define SCB_SHPR3       (*((volatile uint32_t *)0xE000ED20))

// ICSR bit definitions
#define SCB_ICSR_PENDSVSET     (1UL << 28)  ///< PendSV set-pending bit
#define SCB_ICSR_PENDSVCLR     (1UL << 27)  ///< PendSV clear-pending bit
#define SCB_ICSR_PENDSTSET     (1UL << 26)  ///< SysTick set-pending bit
#define SCB_ICSR_PENDSTCLR     (1UL << 25)  ///< SysTick clear-pending bit

// Priority definitions
#define PENDSV_PRIORITY        0xFF        ///< Lowest priority (0xFF = 255)
#define SYSTICK_PRIORITY       0xFF        ///< SysTick priority

// =============================================================================
// Stack Frame Definitions
// =============================================================================

/**
 * @brief Stack frame for Cortex-M33 when an exception occurs.
 * 
 * The order of registers in the stack frame is determined by the CPU.
 * For Cortex-M, the following registers are automatically pushed:
 * - R0-R3
 * - R12
 * - LR
 * - PC
 * - xPSR
 */
typedef struct {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;
} exception_stack_frame_t;

/**
 * @brief Extended stack frame for context switching.
 * 
 * This includes the registers that need to be saved manually:
 * - R4-R11 (callee-saved registers)
 */
typedef struct {
    exception_stack_frame_t exception_frame;
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
} full_stack_frame_t;

// =============================================================================
// Global Variables
// =============================================================================

// Current and next thread TCBs (defined in scheduler)
extern mp_tcb_t *current_thread;
extern mp_tcb_t *next_thread;

// =============================================================================
// Function Prototypes
// =============================================================================

// Assembly functions (defined in irq_handlers.S)
extern void mp_hal_m33_pendsv_handler(void);
extern void mp_hal_m33_systick_handler(void);

// =============================================================================
// Context Switch Initialization
// =============================================================================

void mp_hal_cpu_init(void) {
    // Set PendSV priority to the lowest
    // On Cortex-M, the priority is in the upper 8 bits of the SHPR register
    SCB_SHPR3 = (SCB_SHPR3 & ~(0xFFUL << 24)) | (PENDSV_PRIORITY << 24);
    
    // Set SysTick priority
    SCB_SHPR3 = (SCB_SHPR3 & ~(0xFFUL << 16)) | (SYSTICK_PRIORITY << 16);
    
    // Enable the FPU (if available)
    // nRF54L15 Cortex-M33 has FPU
    SCB_CPACR |= (0xFUL << 20);  // Enable CP10 and CP11 for FPU
    
    // Ensure FPU is enabled in CONTROL register
    __asm volatile (
        "MRS R0, CONTROL\n"
        "ORR R0, R0, #(0x4)\n"  // Enable FPU
        "MSR CONTROL, R0\n"
        : : : "r0"
    );
}

// =============================================================================
// Stack Initialization
// =============================================================================

void mp_hal_cpu_init_stack(mp_tcb_t *tcb, mp_thread_func_t func, void *arg) {
    uint32_t *stack = (uint32_t *)tcb->stack_ptr;
    
    // Align stack to 8-byte boundary (required for Cortex-M)
    stack = (uint32_t *)((uintptr_t)stack & ~0x7);
    
    // Push the initial stack frame
    // Note: We push in reverse order (stack grows downward)
    
    // Start with the exception stack frame
    *(--stack) = 0x01000000;  // xPSR: Thumb mode, no exceptions masked
    *(--stack) = (uint32_t)func;  // PC: Entry point
    *(--stack) = (uint32_t)mp_thread_exit;  // LR: Return address (thread exit)
    *(--stack) = 0x12121212;  // R12
    *(--stack) = 0x03030303;  // R3
    *(--stack) = 0x02020202;  // R2
    *(--stack) = (uint32_t)arg;  // R1: Argument
    *(--stack) = 0x00000000;  // R0
    
    // Push the callee-saved registers (R4-R11)
    *(--stack) = 0x11111111;  // R11
    *(--stack) = 0x10101010;  // R10
    *(--stack) = 0x09090909;  // R9
    *(--stack) = 0x08080808;  // R8
    *(--stack) = 0x07070707;  // R7
    *(--stack) = 0x06060606;  // R6
    *(--stack) = 0x05050505;  // R5
    *(--stack) = 0x04040404;  // R4
    
    // Update the stack pointer in the TCB
    tcb->stack_ptr = (uintptr_t)stack;
}

// =============================================================================
// Context Switch Trigger
// =============================================================================

void mp_hal_cpu_trigger_context_switch(void) {
    // Set the PendSV pending bit
    SCB_ICSR |= SCB_ICSR_PENDSVSET;
    
    // Ensure the write is visible to the CPU
    __asm volatile ("dsb");
    __asm volatile ("isb");
}

// =============================================================================
// Critical Section Management
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

// =============================================================================
// PendSV Handler (C portion)
// =============================================================================

/**
 * @brief PendSV handler - saves/restores context.
 * 
 * This function is called from the PendSV interrupt. It saves the context
 * of the current thread and restores the context of the next thread.
 * 
 * Note: This is the C portion. The actual context save/restore is done in assembly.
 */
void mp_hal_cpu_pendsv_handler(void) {
    // This function is called from the assembly PendSV handler
    // after the context has been saved.
    
    // Update the current and next thread pointers
    // (This is handled in the assembly code)
    
    // If there's a next thread, restore its context
    if (next_thread != NULL) {
        current_thread = next_thread;
        next_thread = NULL;
    }
}

// =============================================================================
// SysTick Handler
// =============================================================================

/**
 * @brief SysTick handler for scheduler tick.
 */
void mp_hal_cpu_systick_handler(void) {
    // Trigger a context switch if needed
    mp_scheduler_tick();
    
    // Trigger PendSV for context switch
    mp_hal_cpu_trigger_context_switch();
}

// =============================================================================
// Core Identification
// =============================================================================

uint32_t mp_hal_nrf54l15_get_core_id(void) {
    // On nRF54L15, we can determine the core by reading a core-specific register
    // For now, we'll assume this is the M33 core
    return CORE_ID_M33;
}

// =============================================================================
// IPC Interrupt Enable for M33
// =============================================================================

void mp_hal_nrf54l15_ipc_enable_interrupts(void) {
    // Enable IPC interrupts in the NVIC
    // nRF54L15-specific: IPC interrupt is typically a system interrupt
    
    // Enable the IPC interrupt in the NVIC
    // Note: Actual implementation depends on nRF54L15 NVIC configuration
    
    // For now, we'll just enable the interrupt at the peripheral level
    NRF54L15_IPC_INTENSET = 0xFFFFFFFF;  // Enable all IPC interrupts
}

// =============================================================================
// Utility Functions
// =============================================================================

uint32_t mp_hal_get_ticks(void) {
    // Return the SysTick counter
    // Note: This is a simple implementation. For production, use a proper tick counter.
    volatile uint32_t *systick_ctrl = (volatile uint32_t *)0xE000E010;
    volatile uint32_t *systick_val = (volatile uint32_t *)0xE000E014;
    
    // Disable interrupts to get a consistent read
    uint32_t status = mp_hal_cpu_enter_critical();
    uint32_t ticks = *systick_val;
    mp_hal_cpu_exit_critical(status);
    
    // SysTick counts down, so we need to invert it
    // Assuming SysTick is configured with a known reload value
    return (0xFFFFFF - ticks);
}

void mp_hal_cpu_delay(uint32_t cycles) {
    // Simple delay loop
    volatile uint32_t i;
    for (i = 0; i < cycles; i++) {
        __asm volatile ("nop");
    }
}
