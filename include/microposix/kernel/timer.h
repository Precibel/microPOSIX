#ifndef MICROPOSIX_TIMER_H
#define MICROPOSIX_TIMER_H

#include <stdint.h>
#include <stdbool.h>

typedef void (*mp_timer_callback_t)(void *arg);

typedef struct mp_timer {
    uint64_t expiry_ms;
    uint32_t period_ms;
    bool periodic;
    mp_timer_callback_t callback;
    void *arg;
    struct mp_timer *next;
} mp_timer_t;

void mp_timer_init(void);
void mp_timer_create(mp_timer_t *timer, uint32_t delay_ms, uint32_t period_ms, bool periodic, mp_timer_callback_t callback, void *arg);
void mp_timer_start(mp_timer_t *timer);
void mp_timer_stop(mp_timer_t *timer);
void mp_timer_tick(void);

#endif // MICROPOSIX_TIMER_H
