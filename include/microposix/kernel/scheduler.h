#ifndef MICROPOSIX_SCHEDULER_H
#define MICROPOSIX_SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>
#include "thread.h"

// Scheduler states
typedef enum {
    MP_SCHED_INITIALIZING,
    MP_SCHED_RUNNING,
    MP_SCHED_SUSPENDED
} mp_sched_state_t;

// Scheduler initialization and start
void mp_sched_init(void);
void mp_sched_start(void);

// Core scheduler functions
void mp_sched_reschedule(void);
void mp_sched_tick(void);

// Thread queue management
void mp_sched_add_thread(mp_tcb_t *tcb);
void mp_sched_remove_thread(mp_tcb_t *tcb);
mp_tcb_t *mp_sched_get_current_thread(void);
mp_tcb_t *mp_sched_get_next_thread(void);

// Critical section management
void mp_sched_enter_critical(void);
void mp_sched_exit_critical(void);

#endif // MICROPOSIX_SCHEDULER_H
