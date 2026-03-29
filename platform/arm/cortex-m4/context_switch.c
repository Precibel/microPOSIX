#include <stdint.h>
#include "microposix/kernel/thread.h"
#include "microposix/kernel/scheduler.h"
#include "microposix/hal/cpu.h"

// Define PendSV priority to the lowest (0xFF)
#define SCB_SHPR3 (*((volatile uint32_t *)0xE000ED20))
#define SCB_ICSR  (*((volatile uint32_t *)0xE000ED04))
#define SCB_ICSR_PENDSVSET (1UL << 28)

void mp_hal_cpu_init_stack(mp_tcb_t *tcb, mp_thread_func_t func, void *arg) {
    uint32_t *stack = (uint32_t *)tcb->stack_ptr;
    
    // Auto-stacked registers by CPU (XPSR, PC, LR, R12, R3, R2, R1, R0)
    *(--stack) = 0x01000000; // XPSR (Thumb mode set)
    *(--stack) = (uint32_t)func; // PC (Entry point)
    *(--stack) = (uint32_t)mp_thread_exit; // LR (Return address)
    *(--stack) = 0; // R12
    *(--stack) = 0; // R3
    *(--stack) = 0; // R2
    *(--stack) = 0; // R1
    *(--stack) = (uint32_t)arg; // R0
    
    // Software-saved registers (R11-R4)
    *(--stack) = 0x11111111; // R11
    *(--stack) = 0x10101010; // R10
    *(--stack) = 0x09090909; // R9
    *(--stack) = 0x08080808; // R8
    *(--stack) = 0x07070707; // R7
    *(--stack) = 0x06060606; // R6
    *(--stack) = 0x05050505; // R5
    *(--stack) = 0x04040404; // R4
    
    // Update stack pointer in TCB
    tcb->stack_ptr = (uintptr_t)stack;
}

void mp_hal_cpu_trigger_context_switch(void) {
    // Set PendSV bit to pending
    SCB_ICSR |= SCB_ICSR_PENDSVSET;
}

uint32_t mp_hal_cpu_enter_critical(void) {
    uint32_t status;
    __asm volatile ("mrs %0, primask" : "=r" (status));
    __asm volatile ("cpsid i");
    return status;
}

void mp_hal_cpu_exit_critical(uint32_t status) {
    __asm volatile ("msr primask, %0" :: "r" (status));
}

// PendSV Handler implementation (C portion)
// Note: The main logic is in the assembly file
void mp_hal_cpu_pendsv_handler(void) {
    // This function will be called from assembly after saving R4-R11
    // It should update current_thread and next_thread
}
