#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "microposix/kernel/mq.h"
#include "microposix/kernel/scheduler.h"
#include "microposix/kernel/thread.h"

int mp_mq_init(mp_mq_t *mq, void *buffer, size_t msg_size, size_t max_msgs) {
    if (!mq || !buffer) return -1;
    
    mq->buffer = buffer;
    mq->msg_size = msg_size;
    mq->max_msgs = max_msgs;
    mq->cur_msgs = 0;
    mq->head = 0;
    mq->tail = 0;
    mq->waiting_send = NULL;
    mq->waiting_recv = NULL;
    
    return 0;
}

int mp_mq_send(mp_mq_t *mq, const void *msg, uint32_t timeout_ms) {
    (void)timeout_ms;
    // mp_sched_enter_critical();
    
    if (mq->cur_msgs >= mq->max_msgs) {
        // Block sending thread (simplified)
        // mp_sched_exit_critical();
        return -1;
    }
    
    uint8_t *dest = (uint8_t *)mq->buffer + (mq->head * mq->msg_size);
    memcpy(dest, msg, mq->msg_size);
    
    mq->head = (mq->head + 1) % mq->max_msgs;
    mq->cur_msgs++;
    
    // Wake up one receiving thread if any
    // ...
    
    // mp_sched_exit_critical();
    return 0;
}

int mp_mq_receive(mp_mq_t *mq, void *msg, uint32_t timeout_ms) {
    (void)timeout_ms;
    // mp_sched_enter_critical();
    
    if (mq->cur_msgs == 0) {
        // Block receiving thread (simplified)
        // mp_sched_exit_critical();
        return -1;
    }
    
    uint8_t *src = (uint8_t *)mq->buffer + (mq->tail * mq->msg_size);
    memcpy(msg, src, mq->msg_size);
    
    mq->tail = (mq->tail + 1) % mq->max_msgs;
    mq->cur_msgs--;
    
    // Wake up one sending thread if any
    // ...
    
    // mp_sched_exit_critical();
    return 0;
}
