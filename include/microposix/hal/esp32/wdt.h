#ifndef MICROPOSIX_HAL_ESP32_WDT_H
#define MICROPOSIX_HAL_ESP32_WDT_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_task_wdt.h"
#include "driver/timer.h"

// ESP32 has multiple watchdog options:
// 1. Task Watchdog (FreeRTOS-based, per-task)
// 2. Hardware Watchdog (RTC WDT)
// 3. Interrupt Watchdog (using timer)

// Watchdog timeout configuration
#define MP_HAL_ESP32_WDT_TIMEOUT_MS 5000  // 5 seconds

// Initialize hardware watchdog (RTC WDT)
int mp_hal_esp32_wdt_init(uint32_t timeout_ms);

// Feed/refresh hardware watchdog
void mp_hal_esp32_wdt_feed(void);

// Disable hardware watchdog
void mp_hal_esp32_wdt_disable(void);

// Check if watchdog caused last reset
bool mp_hal_esp32_wdt_was_reset_cause(void);

// Get watchdog reset cause
uint32_t mp_hal_esp32_wdt_get_reset_cause(void);

// Initialize task watchdog for a specific task
int mp_hal_esp32_task_wdt_init(uint32_t timeout_ms);

// Register a task with the task watchdog
void mp_hal_esp32_task_wdt_add(xTaskHandle task);

// Reset task watchdog for a specific task
void mp_hal_esp32_task_wdt_reset(xTaskHandle task);

// Initialize per-thread software watchdog (microPOSIX style)
// This uses a timer to check thread check-ins
int mp_hal_esp32_thread_wdt_init(uint32_t timeout_ms);

// Thread check-in function
void mp_hal_esp32_thread_wdt_checkin(uint32_t thread_id);

// Get thread that failed to check in
uint32_t mp_hal_esp32_thread_wdt_get_offending_thread(void);

// Set callback for watchdog timeout
void mp_hal_esp32_wdt_set_callback(void (*callback)(void));

#endif // MICROPOSIX_HAL_ESP32_WDT_H
