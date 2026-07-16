/**
 * ESP32 Watchdog HAL Implementation
 * 
 * This file provides watchdog functions for ESP32.
 */

#include <stdint.h>
#include <stdbool.h>
#include "microposix/hal/esp32/wdt.h"
#include "esp_task_wdt.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/timer.h"

static const char *TAG = "mp_wdt";

// Watchdog callback
static void (*wdt_callback)(void) = NULL;

// Thread watchdog structure
typedef struct {
    uint32_t thread_id;
    uint64_t last_checkin;
    bool active;
} mp_hal_esp32_thread_wdt_t;

// Thread watchdog table
#define MAX_THREAD_WDT 32
static mp_hal_esp32_thread_wdt_t thread_wdt_table[MAX_THREAD_WDT] = {0};

// Watchdog timeout
static uint32_t wdt_timeout_ms = MP_HAL_ESP32_WDT_TIMEOUT_MS;

// Initialize hardware watchdog (RTC WDT)
int mp_hal_esp32_wdt_init(uint32_t timeout_ms) {
    // ESP32 has a hardware watchdog timer (RTC WDT)
    // This is enabled by default in ESP-IDF
    
    wdt_timeout_ms = timeout_ms;
    
    // Enable RTC WDT
    esp_err_t err = esp_task_wdt_init(timeout_ms / 1000, true);  // Convert ms to seconds
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize task watchdog: %s", esp_err_to_name(err));
        return -1;
    }
    
    // Add current task to watchdog
    esp_task_wdt_add(xTaskGetCurrentTaskHandle());
    
    ESP_LOGI(TAG, "Hardware watchdog initialized with timeout %d ms", timeout_ms);
    return 0;
}

// Feed/refresh hardware watchdog
void mp_hal_esp32_wdt_feed(void) {
    // Feed the task watchdog
    esp_task_wdt_reset();
}

// Disable hardware watchdog
void mp_hal_esp32_wdt_disable(void) {
    esp_task_wdt_delete(xTaskGetCurrentTaskHandle());
    ESP_LOGI(TAG, "Hardware watchdog disabled");
}

// Check if watchdog caused last reset
bool mp_hal_esp32_wdt_was_reset_cause(void) {
    esp_reset_reason_t reason = esp_reset_reason();
    return (reason == ESP_RST_TASK_WDT || reason == ESP_RST_INT_WDT);
}

// Get watchdog reset cause
uint32_t mp_hal_esp32_wdt_get_reset_cause(void) {
    esp_reset_reason_t reason = esp_reset_reason();
    
    switch (reason) {
        case ESP_RST_UNKNOWN:    return 0;
        case ESP_RST_POWERON:    return 1;
        case ESP_RST_EXT:        return 2;
        case ESP_RST_SW:         return 3;
        case ESP_RST_PANIC:      return 4;
        case ESP_RST_INT_WDT:    return 5;
        case ESP_RST_TASK_WDT:   return 6;
        case ESP_RST_WDT:        return 7;
        case ESP_RST_DEEPSLEEP:  return 8;
        case ESP_RST_BROWNOUT:   return 9;
        case ESP_RST_SDIO:       return 10;
        default:                 return 0;
    }
}

// Initialize task watchdog for a specific task
int mp_hal_esp32_task_wdt_init(uint32_t timeout_ms) {
    wdt_timeout_ms = timeout_ms;
    
    esp_err_t err = esp_task_wdt_init(timeout_ms / 1000, true);
    if (err != ESP_OK) {
        return -1;
    }
    
    return 0;
}

// Register a task with the task watchdog
void mp_hal_esp32_task_wdt_add(xTaskHandle task) {
    esp_task_wdt_add(task);
}

// Reset task watchdog for a specific task
void mp_hal_esp32_task_wdt_reset(xTaskHandle task) {
    esp_task_wdt_reset();
}

// Initialize per-thread software watchdog
int mp_hal_esp32_thread_wdt_init(uint32_t timeout_ms) {
    wdt_timeout_ms = timeout_ms;
    
    // Initialize all thread watchdog entries
    for (int i = 0; i < MAX_THREAD_WDT; i++) {
        thread_wdt_table[i].thread_id = 0;
        thread_wdt_table[i].last_checkin = 0;
        thread_wdt_table[i].active = false;
    }
    
    // Create a timer to check thread check-ins
    // This would run as a separate task in a real implementation
    
    ESP_LOGI(TAG, "Thread watchdog initialized with timeout %d ms", timeout_ms);
    return 0;
}

// Thread check-in function
void mp_hal_esp32_thread_wdt_checkin(uint32_t thread_id) {
    // Find the thread in the table
    for (int i = 0; i < MAX_THREAD_WDT; i++) {
        if (thread_wdt_table[i].thread_id == thread_id && thread_wdt_table[i].active) {
            thread_wdt_table[i].last_checkin = esp_timer_get_time();
            return;
        }
    }
    
    // If not found, add it
    for (int i = 0; i < MAX_THREAD_WDT; i++) {
        if (!thread_wdt_table[i].active) {
            thread_wdt_table[i].thread_id = thread_id;
            thread_wdt_table[i].last_checkin = esp_timer_get_time();
            thread_wdt_table[i].active = true;
            return;
        }
    }
    
    ESP_LOGW(TAG, "No free slot for thread %d in watchdog table", thread_id);
}

// Get thread that failed to check in
uint32_t mp_hal_esp32_thread_wdt_get_offending_thread(void) {
    uint64_t current_time = esp_timer_get_time();
    
    for (int i = 0; i < MAX_THREAD_WDT; i++) {
        if (thread_wdt_table[i].active) {
            uint64_t elapsed = current_time - thread_wdt_table[i].last_checkin;
            if (elapsed > (wdt_timeout_ms * 1000)) {  // Convert ms to us
                return thread_wdt_table[i].thread_id;
            }
        }
    }
    
    return 0;  // No offending thread
}

// Set callback for watchdog timeout
void mp_hal_esp32_wdt_set_callback(void (*callback)(void)) {
    wdt_callback = callback;
}

// Watchdog monitoring task
static void mp_hal_esp32_wdt_monitor_task(void *arg) {
    (void)arg;
    
    while (1) {
        // Check for thread watchdog timeouts
        uint32_t offending_thread = mp_hal_esp32_thread_wdt_get_offending_thread();
        if (offending_thread != 0) {
            ESP_LOGE(TAG, "Thread %d failed to check in!", offending_thread);
            if (wdt_callback != NULL) {
                wdt_callback();
            }
        }
        
        // Feed the hardware watchdog
        mp_hal_esp32_wdt_feed();
        
        // Delay for a portion of the timeout
        vTaskDelay(pdMS_TO_TICKS(wdt_timeout_ms / 10));
    }
}

// Start watchdog monitoring task
void mp_hal_esp32_wdt_start_monitor(void) {
    xTaskCreate(mp_hal_esp32_wdt_monitor_task, "wdt_monitor", 2048, NULL, 5, NULL);
}
