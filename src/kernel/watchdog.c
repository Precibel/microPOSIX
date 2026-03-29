#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "microposix/kernel/watchdog.h"
#include "microposix/kernel/scheduler.h"
#include "microposix/kernel/thread.h"
#include "microposix/debug/log.h"

#define MAX_WDT_ENTRIES 32
static mp_wdt_entry_t wdt_table[MAX_WDT_ENTRIES];
static uint32_t wdt_timeout_ms = 5000;

void mp_wdt_init(uint32_t timeout_ms) {
    wdt_timeout_ms = timeout_ms;
    for (int i = 0; i < MAX_WDT_ENTRIES; i++) {
        wdt_table[i].active = false;
    }
}

void mp_wdt_register(uint32_t thread_id) {
    for (int i = 0; i < MAX_WDT_ENTRIES; i++) {
        if (!wdt_table[i].active) {
            wdt_table[i].thread_id = thread_id;
            wdt_table[i].last_check_in = mp_clock_get_monotonic_ms();
            wdt_table[i].active = true;
            return;
        }
    }
}

void mp_wdt_check_in(uint32_t thread_id) {
    for (int i = 0; i < MAX_WDT_ENTRIES; i++) {
        if (wdt_table[i].active && wdt_table[i].thread_id == thread_id) {
            wdt_table[i].last_check_in = mp_clock_get_monotonic_ms();
            return;
        }
    }
}

void mp_wdt_supervisor_tick(void) {
    uint32_t now = mp_clock_get_monotonic_ms();
    for (int i = 0; i < MAX_WDT_ENTRIES; i++) {
        if (wdt_table[i].active) {
            if (now - wdt_table[i].last_check_in > wdt_timeout_ms) {
                MP_LOGE("WDT", "Watchdog timeout on thread ID %d!", wdt_table[i].thread_id);
                // In a real OS, this would trigger a system reset or panic
            }
        }
    }
}
