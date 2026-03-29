#ifndef MICROPOSIX_CPU_H
#define MICROPOSIX_CPU_H

#include <stdint.h>
#include "microposix/kernel/thread.h"

// Initialize the stack for a new thread
void mp_hal_cpu_init_stack(mp_tcb_t *tcb, mp_thread_func_t func, void *arg);

// Trigger a context switch (usually by triggering PendSV)
void mp_hal_cpu_trigger_context_switch(void);

// Enter/Exit critical sections
uint32_t mp_hal_cpu_enter_critical(void);
void mp_hal_cpu_exit_critical(uint32_t status);

#endif // MICROPOSIX_CPU_H
