#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "microposix/debug/log.h"
#include "microposix/kernel/thread.h"
#include "microposix/kernel/scheduler.h"

typedef struct {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r12;
    uint32_t lr;
    uint32_t pc;
    uint32_t xpsr;
} mp_fault_frame_t;

void mp_panic(const char *reason, mp_fault_frame_t *frame) {
    mp_tcb_t *current = mp_sched_get_current_thread();
    
    MP_LOGC("PANIC", "System Panic: %s", reason);
    if (current) {
        MP_LOGC("PANIC", "Current Thread: %s (ID: %d)", current->name, current->id);
    }
    
    if (frame) {
        MP_LOGC("PANIC", "Register Dump:");
        MP_LOGC("PANIC", " R0:  0x%08X  R1:  0x%08X", frame->r0, frame->r1);
        MP_LOGC("PANIC", " R2:  0x%08X  R3:  0x%08X", frame->r2, frame->r3);
        MP_LOGC("PANIC", " R12: 0x%08X  LR:  0x%08X", frame->r12, frame->lr);
        MP_LOGC("PANIC", " PC:  0x%08X  XPSR:0x%08X", frame->pc, frame->xpsr);
    }
    
    // In a real system, we'd write to retained SRAM and reset
    exit(1);
}

// ARM-specific HardFault_Handler
void HardFault_Handler(void) {
    // This would be called from assembly with the stack pointer
    // For now, it's just a placeholder
    mp_panic("HardFault", NULL);
}
