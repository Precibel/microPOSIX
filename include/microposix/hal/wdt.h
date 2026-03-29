#ifndef MICROPOSIX_WDT_H
#define MICROPOSIX_WDT_H

#include <stdint.h>

// WDT management functions
void mp_hal_wdt_init(uint32_t timeout_ms);
void mp_hal_wdt_feed(void);
void mp_hal_wdt_reset(void);

#endif // MICROPOSIX_WDT_H
