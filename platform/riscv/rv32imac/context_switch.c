#include <stdint.h>
#include "microposix/kernel/thread.h"
#include "microposix/kernel/scheduler.h"
#include "microposix/hal/cpu.h"

// RISC-V Stack Frame (32 registers)
// R0 is zero, so we don't save it.
// PC, RA, SP, GP, TP, T0-T2, S0-S1, A0-A7, S2-S11, T3-T6
#define RISCV_STACK_FRAME_SIZE 31

void mp_hal_cpu_init_stack(mp_tcb_t *tcb, mp_thread_func_t func, void *arg) {
    uint32_t *stack = (uint32_t *)tcb->stack_ptr;
    
    // Initialize the stack frame
    for (int i = 0; i < RISCV_STACK_FRAME_SIZE; i++) {
        *(--stack) = 0;
    }
    
    // Set return address to the thread function entry
    stack[30] = (uint32_t)func; // PC-equivalent
    stack[1] = (uint32_t)mp_thread_exit; // RA (Return address)
    stack[10] = (uint32_t)arg; // A0 (Argument)
    
    // Update stack pointer in TCB
    tcb->stack_ptr = (uintptr_t)stack;
}

void mp_hal_cpu_trigger_context_switch(void) {
    // Trigger software interrupt or environment call (ECALL)
    // For RISC-V, this usually involves setting the machine-level software interrupt bit (MSIP)
}

uint32_t mp_hal_cpu_enter_critical(void) {
    uint32_t status;
    __asm volatile ("csrr %0, mstatus" : "=r" (status));
    __asm volatile ("csrci mstatus, 8"); // Disable machine interrupts (MIE)
    return status;
}

void mp_hal_cpu_exit_critical(uint32_t status) {
    __asm volatile ("csrw mstatus, %0" :: "r" (status));
}
