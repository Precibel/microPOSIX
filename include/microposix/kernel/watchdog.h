#ifndef MICROPOSIX_WATCHDOG_H
#define MICROPOSIX_WATCHDOG_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t thread_id;
    uint32_t last_check_in;
    bool active;
} mp_wdt_entry_t;

void mp_wdt_init(uint32_t timeout_ms);
void mp_wdt_register(uint32_t thread_id);
void mp_wdt_check_in(uint32_t thread_id);
void mp_wdt_supervisor_tick(void);

#endif // MICROPOSIX_WATCHDOG_H
