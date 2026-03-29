#ifndef MICROPOSIX_MQ_H
#define MICROPOSIX_MQ_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "microposix/kernel/ipc.h"

int mp_mq_init(mp_mq_t *mq, void *buffer, size_t msg_size, size_t max_msgs);
int mp_mq_send(mp_mq_t *mq, const void *msg, uint32_t timeout_ms);
int mp_mq_receive(mp_mq_t *mq, void *msg, uint32_t timeout_ms);

#endif // MICROPOSIX_MQ_H
