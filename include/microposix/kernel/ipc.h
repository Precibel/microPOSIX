#ifndef MICROPOSIX_IPC_H
#define MICROPOSIX_IPC_H

#include <stdint.h>
#include <stdbool.h>
#include "microposix/kernel/thread.h"

// Mutex definition
typedef struct mp_mutex {
    mp_tcb_t *owner;
    mp_tcb_t *waiting_list;
    uint8_t count;
    bool priority_inheritance;
} mp_mutex_t;

// Semaphore definition
typedef struct mp_sem {
    uint32_t count;
    mp_tcb_t *waiting_list;
} mp_sem_t;

// Message queue definition
typedef struct mp_mq {
    void *buffer;
    size_t msg_size;
    size_t max_msgs;
    size_t cur_msgs;
    size_t head;
    size_t tail;
    mp_tcb_t *waiting_send;
    mp_tcb_t *waiting_recv;
} mp_mq_t;

#endif // MICROPOSIX_IPC_H
