#ifndef MICROPOSIX_PTHREAD_H
#define MICROPOSIX_PTHREAD_H

#include <stdint.h>
#include <stddef.h>
#include "microposix/kernel/thread.h"

// pthread_t is just mp_thread_id_t
typedef mp_thread_id_t mp_pthread_t;

// pthread_attr_t is just mp_thread_attr_t
typedef mp_thread_attr_t mp_pthread_attr_t;

// pthread_mutex_t is a pointer to the internal mutex structure
typedef struct mp_mutex *mp_pthread_mutex_t;

// pthread_cond_t is a pointer to the internal condition variable structure
typedef struct mp_cond *mp_pthread_cond_t;

// Function prototypes
int pthread_create(mp_pthread_t *thread, const mp_pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
int pthread_join(mp_pthread_t thread, void **retval);
void pthread_exit(void *retval);
mp_pthread_t pthread_self(void);

// Mutex functions
int pthread_mutex_init(mp_pthread_mutex_t *mutex, const void *attr);
int pthread_mutex_lock(mp_pthread_mutex_t *mutex);
int pthread_mutex_unlock(mp_pthread_mutex_t *mutex);
int pthread_mutex_destroy(mp_pthread_mutex_t *mutex);

// Condition variable functions
int pthread_cond_init(mp_pthread_cond_t *cond, const void *attr);
int pthread_cond_wait(mp_pthread_cond_t *cond, mp_pthread_mutex_t *mutex);
int pthread_cond_signal(mp_pthread_cond_t *cond);
int pthread_cond_broadcast(mp_pthread_cond_t *cond);
int pthread_cond_destroy(mp_pthread_cond_t *cond);

#endif // MICROPOSIX_PTHREAD_H
