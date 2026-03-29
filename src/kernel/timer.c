#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "microposix/kernel/timer.h"
#include "microposix/kernel/thread.h"
#include "microposix/kernel/scheduler.h"

static mp_timer_t *timer_list = NULL;

void mp_timer_init(void) {
    timer_list = NULL;
}

void mp_timer_create(mp_timer_t *timer, uint32_t delay_ms, uint32_t period_ms, bool periodic, mp_timer_callback_t callback, void *arg) {
    timer->expiry_ms = mp_clock_get_monotonic_ms() + delay_ms;
    timer->period_ms = period_ms;
    timer->periodic = periodic;
    timer->callback = callback;
    timer->arg = arg;
    timer->next = NULL;
}

void mp_timer_start(mp_timer_t *timer) {
    // mp_sched_enter_critical();
    
    // Check if already in list
    mp_timer_t *curr = timer_list;
    while (curr) {
        if (curr == timer) {
            // mp_sched_exit_critical();
            return;
        }
        curr = curr->next;
    }
    
    // Add to list
    timer->next = timer_list;
    timer_list = timer;
    
    // mp_sched_exit_critical();
}

void mp_timer_stop(mp_timer_t *timer) {
    // mp_sched_enter_critical();
    
    mp_timer_t *curr = timer_list;
    mp_timer_t *prev = NULL;
    
    while (curr) {
        if (curr == timer) {
            if (prev == NULL) {
                timer_list = curr->next;
            } else {
                prev->next = curr->next;
            }
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    
    // mp_sched_exit_critical();
}

void mp_timer_tick(void) {
    uint64_t now = mp_clock_get_monotonic_ms();
    mp_timer_t *curr = timer_list;
    
    while (curr) {
        if (now >= curr->expiry_ms) {
            if (curr->callback) {
                curr->callback(curr->arg);
            }
            
            if (curr->periodic) {
                curr->expiry_ms = now + curr->period_ms;
            } else {
                // Remove from list
                mp_timer_stop(curr);
            }
        }
        curr = curr->next;
    }
}
