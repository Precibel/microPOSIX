#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "microposix/kernel/scheduler.h"
#include "microposix/kernel/thread.h"

// Static storage for TCBs (Profile A: 32 threads, Profile B: 8 threads)
#if MICROPOSIX_PROFILE == 1
#define MAX_THREADS 32
#else
#define MAX_THREADS 8
#endif

static mp_tcb_t tcb_pool[MAX_THREADS];
static bool tcb_used[MAX_THREADS];

// Scheduler state
static mp_tcb_t *current_thread = NULL;
static mp_tcb_t *next_thread = NULL;
static mp_tcb_t *ready_list = NULL;
static mp_sched_state_t sched_state = MP_SCHED_INITIALIZING;

// Monotonic clock
static uint64_t monotonic_ms = 0;

uint64_t mp_clock_get_monotonic_ms(void) {
    return monotonic_ms;
}

uint64_t mp_clock_get_monotonic_us(void) {
    return monotonic_ms * 1000;
}

// Idle thread TCB
static mp_tcb_t idle_tcb;
static uint32_t idle_stack[256];

// Tickless Idle: calculate sleep duration based on timers and BLE
uint32_t mp_sched_calculate_sleep(void) {
    uint32_t sleep_ticks = 0xFFFFFFFF; // Max sleep
    
    // Check thread sleep timers
    mp_tcb_t *curr = ready_list;
    while (curr) {
        if (curr->state == MP_THREAD_SLEEPING) {
            if (curr->sleep_ticks < sleep_ticks) {
                sleep_ticks = curr->sleep_ticks;
            }
        }
        curr = curr->next;
    }
    
    // Check BLE oracle (simulated)
    // uint32_t ble_ticks = mp_ble_get_next_anchor_ticks();
    // if (ble_ticks < sleep_ticks) sleep_ticks = ble_ticks;
    
    return (sleep_ticks == 0xFFFFFFFF) ? 0 : sleep_ticks;
}

void *idle_thread_func(void *arg) {
    (void)arg;
    while (1) {
        uint32_t sleep = mp_sched_calculate_sleep();
        if (sleep > 0) {
            // Enter tickless idle (simulated)
            // mp_hal_cpu_enter_sleep(sleep);
        }
        
        // Wait for interrupt (WFI)
        __asm__ volatile ("wfi");
    }
}

void mp_sched_init(void) {
    // Initialize TCB pool
    memset(tcb_pool, 0, sizeof(tcb_pool));
    memset(tcb_used, 0, sizeof(tcb_used));
    
    // Create idle thread
    idle_tcb.id = 0;
    strcpy(idle_tcb.name, "idle");
    idle_tcb.priority = 0;
    idle_tcb.state = MP_THREAD_READY;
    idle_tcb.stack_base = (uintptr_t)idle_stack;
    idle_tcb.stack_size = sizeof(idle_stack);
    idle_tcb.stack_ptr = (uintptr_t)(idle_stack + 256 - 1); // Point to top of stack
    
    ready_list = &idle_tcb;
    current_thread = &idle_tcb;
    
    sched_state = MP_SCHED_RUNNING;
}

void mp_sched_start(void) {
    // Start the first thread (architecture-specific)
    // For now, just set state to running
    sched_state = MP_SCHED_RUNNING;
}

void mp_sched_reschedule(void) {
    // Basic priority-based scheduler
    mp_tcb_t *best_thread = NULL;
    uint8_t highest_priority = 0;
    
    mp_tcb_t *curr = ready_list;
    while (curr != NULL) {
        if (curr->state == MP_THREAD_READY || curr->state == MP_THREAD_RUNNING) {
            if (curr->priority >= highest_priority) {
                highest_priority = curr->priority;
                best_thread = curr;
            }
        }
        curr = curr->next;
    }
    
    if (best_thread != current_thread) {
        next_thread = best_thread;
        // Trigger context switch (architecture-specific)
    }
}

void mp_sched_tick(void) {
    // Increment monotonic clock
    monotonic_ms++;
    
    // Update sleep timers
    mp_tcb_t *curr = ready_list;
    while (curr != NULL) {
        if (curr->state == MP_THREAD_SLEEPING) {
            if (curr->sleep_ticks > 0) {
                curr->sleep_ticks--;
            }
            if (curr->sleep_ticks == 0) {
                curr->state = MP_THREAD_READY;
            }
        }
        
        // Stack High-Water-Mark Scanner (HWM)
        if (curr->stack_base != 0) {
            uint8_t *stack = (uint8_t *)curr->stack_base;
            uint32_t unused = 0;
            while (unused < curr->stack_size && stack[unused] == 0xAA) {
                unused++;
            }
            curr->stack_hwm = curr->stack_size - unused;
        }
        
        curr = curr->next;
    }
    
    // Preempt if necessary
    mp_sched_reschedule();
}

mp_tcb_t *mp_sched_get_current_thread(void) {
    return current_thread;
}

mp_tcb_t *mp_sched_get_next_thread(void) {
    return next_thread;
}

void mp_sched_add_thread(mp_tcb_t *tcb) {
    // Add to ready list
    tcb->next = ready_list;
    ready_list = tcb;
}

void mp_sched_remove_thread(mp_tcb_t *tcb) {
    // Remove from ready list
    mp_tcb_t *curr = ready_list;
    mp_tcb_t *prev = NULL;
    
    while (curr != NULL) {
        if (curr == tcb) {
            if (prev == NULL) {
                ready_list = curr->next;
            } else {
                prev->next = curr->next;
            }
            break;
        }
        prev = curr;
        curr = curr->next;
    }
}
