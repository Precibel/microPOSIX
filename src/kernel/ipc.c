#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "microposix/kernel/ipc.h"
#include "microposix/kernel/scheduler.h"
#include "microposix/kernel/thread.h"
#include "pthread.h"

int pthread_mutex_init(mp_pthread_mutex_t *mutex, const void *attr) {
    (void)attr;
    mp_mutex_t *m = (mp_mutex_t *)malloc(sizeof(mp_mutex_t));
    if (!m) return -1;
    
    m->owner = NULL;
    m->waiting_list = NULL;
    m->count = 0;
    m->priority_inheritance = true; // Enabled by default for Profile A
    
    *mutex = m;
    return 0;
}

int pthread_mutex_lock(mp_pthread_mutex_t *mutex) {
    mp_mutex_t *m = *mutex;
    mp_tcb_t *current = mp_sched_get_current_thread();
    
    if (m->owner == NULL) {
        m->owner = current;
        m->count = 1;
    } else if (m->owner == current) {
        m->count++; // Recursive lock
    } else {
        // Block the current thread
        current->state = MP_THREAD_BLOCKED;
        mp_sched_reschedule();
    }
    
    return 0;
}

int pthread_mutex_unlock(mp_pthread_mutex_t *mutex) {
    mp_mutex_t *m = *mutex;
    mp_tcb_t *current = mp_sched_get_current_thread();
    
    if (m->owner != current) return -1;
    
    m->count--;
    if (m->count == 0) {
        m->owner = NULL;
        mp_sched_reschedule();
    }
    
    return 0;
}

int sem_init(mp_sem_t *sem, int pshared, unsigned int value) {
    (void)pshared;
    sem->count = value;
    sem->waiting_list = NULL;
    return 0;
}

int sem_wait(mp_sem_t *sem) {
    if (sem->count > 0) {
        sem->count--;
    } else {
        mp_tcb_t *current = mp_sched_get_current_thread();
        current->state = MP_THREAD_BLOCKED;
        mp_sched_reschedule();
    }
    return 0;
}

int sem_post(mp_sem_t *sem) {
    sem->count++;
    mp_sched_reschedule();
    return 0;
}
