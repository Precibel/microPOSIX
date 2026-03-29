#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "microposix/kernel/thread.h"
#include "microposix/kernel/scheduler.h"

#ifndef MICROPOSIX_STACK_SIZE_DEFAULT
#define MICROPOSIX_STACK_SIZE_DEFAULT 1024
#endif

// Static pool for TCBs
#if MICROPOSIX_PROFILE == 1
#define MAX_THREADS 32
#else
#define MAX_THREADS 8
#endif

static mp_tcb_t tcb_pool[MAX_THREADS];
static bool tcb_used[MAX_THREADS];

// Counter for thread IDs
static mp_thread_id_t next_thread_id = 1;

mp_thread_id_t mp_thread_create(mp_thread_func_t func, void *arg, const mp_thread_attr_t *attr) {
    int idx = -1;
    
    // Find free TCB
    for (int i = 0; i < MAX_THREADS; i++) {
        if (!tcb_used[i]) {
            idx = i;
            break;
        }
    }
    
    if (idx == -1) {
        return 0; // No free TCBs
    }
    
    mp_tcb_t *tcb = &tcb_pool[idx];
    tcb_used[idx] = true;
    
    // Initialize TCB
    tcb->id = next_thread_id++;
    if (attr && attr->name) {
        strncpy(tcb->name, attr->name, sizeof(tcb->name) - 1);
    } else {
        snprintf(tcb->name, sizeof(tcb->name), "thread_%d", tcb->id);
    }
    
    tcb->priority = attr ? attr->priority : 1;
    tcb->state = MP_THREAD_READY;
    tcb->stack_size = attr ? attr->stack_size : MICROPOSIX_STACK_SIZE_DEFAULT;
    
    // Allocate stack (for now, just use static buffer for simplicity, or we can use malloc later)
    // Actually, for a real RTOS, we might want to use a pool or the heap.
    // In this example, let's assume the user provides a buffer or we allocate from a global heap.
    // For simplicity, let's just use a static buffer for each thread.
    static uint32_t thread_stacks[MAX_THREADS][1024];
    tcb->stack_base = (uintptr_t)thread_stacks[idx];
    tcb->stack_ptr = (uintptr_t)(thread_stacks[idx] + 1024 - 1); // Top of stack
    
    // Initialize stack for the architecture (to be implemented in HAL)
    // mp_hal_cpu_init_stack(tcb, func, arg);
    
    mp_sched_add_thread(tcb);
    mp_sched_reschedule();
    
    return tcb->id;
}

void mp_thread_yield(void) {
    mp_sched_reschedule();
}

void mp_thread_sleep(uint32_t ticks) {
    mp_tcb_t *current = mp_sched_get_current_thread();
    if (current) {
        current->state = MP_THREAD_SLEEPING;
        current->sleep_ticks = ticks;
        mp_sched_reschedule();
    }
}

void mp_thread_exit(void *retval) {
    (void)retval;
    mp_tcb_t *current = mp_sched_get_current_thread();
    if (current) {
        current->state = MP_THREAD_TERMINATED;
        // In a real implementation, we would mark the TCB as free or handle joining
        mp_sched_reschedule();
    }
}

int mp_thread_join(mp_thread_id_t tid, void **retval) {
    (void)retval;
    // Find thread by ID
    mp_tcb_t *target = NULL;
    for (int i = 0; i < MAX_THREADS; i++) {
        if (tcb_used[i] && tcb_pool[i].id == tid) {
            target = &tcb_pool[i];
            break;
        }
    }
    
    if (!target) return -1;
    
    // Simple busy wait for simulation, in real OS this would block the caller
    while (target->state != MP_THREAD_TERMINATED) {
        mp_thread_yield();
    }
    
    return 0;
}
