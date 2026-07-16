/**
 * microPOSIX ESP32 Demo - Blinking LED Application
 * 
 * This file contains the blinking LED application using microPOSIX API.
 * It demonstrates how to create threads, use synchronization primitives,
 * and interact with hardware using the microPOSIX HAL.
 */

#include <stdint.h>
#include <stdbool.h>
#include "microposix/kernel/thread.h"
#include "microposix/kernel/scheduler.h"
#include "microposix/kernel/ipc.h"
#include "microposix/debug/log.h"
#include "microposix/hal/esp32/gpio.h"

// Application configuration
#define BLINK_LED_PIN MP_HAL_ESP32_GPIO_LED_BUILTIN
#define BLINK_INTERVAL_MS 500

// Thread priorities
#define BLINK_TASK_PRIORITY 10
#define MONITOR_TASK_PRIORITY 5

// Global variables
static volatile bool app_running = true;

// Forward declarations
void blink_task(void *arg);
void monitor_task(void *arg);

/**
 * @brief Initialize the application
 */
int app_blink_init(void) {
    MP_LOGI("APP", "Initializing blinking LED application");
    
    // Initialize LED GPIO
    int err = mp_hal_esp32_gpio_init(BLINK_LED_PIN, MP_HAL_ESP32_GPIO_OUTPUT, MP_HAL_ESP32_GPIO_NOPULL);
    if (err != 0) {
        MP_LOGE("APP", "Failed to initialize LED GPIO");
        return -1;
    }
    
    // Set LED to off initially
    mp_hal_esp32_gpio_write(BLINK_LED_PIN, 0);
    
    MP_LOGI("APP", "LED GPIO initialized on pin %d", BLINK_LED_PIN);
    
    return 0;
}

/**
 * @brief Blink task - toggles the LED at regular intervals
 */
void blink_task(void *arg) {
    (void)arg;
    
    MP_LOGI("APP", "Blink task started");
    
    int count = 0;
    while (app_running) {
        // Turn LED on
        mp_hal_esp32_gpio_write(BLINK_LED_PIN, 1);
        MP_LOGD("APP", "LED ON (count: %d)", count);
        
        // Wait for half the interval
        mp_thread_sleep(BLINK_INTERVAL_MS / 2);
        
        // Turn LED off
        mp_hal_esp32_gpio_write(BLINK_LED_PIN, 0);
        MP_LOGD("APP", "LED OFF");
        
        // Wait for the other half
        mp_thread_sleep(BLINK_INTERVAL_MS / 2);
        
        count++;
    }
    
    MP_LOGI("APP", "Blink task exiting");
    mp_thread_exit(NULL);
}

/**
 * @brief Monitor task - logs system status periodically
 */
void monitor_task(void *arg) {
    (void)arg;
    
    MP_LOGI("APP", "Monitor task started");
    
    while (app_running) {
        // Get uptime
        uint64_t uptime_ms = mp_clock_get_monotonic_ms();
        
        // Get current thread info
        mp_tcb_t *current = mp_sched_get_current_thread();
        
        if (current != NULL) {
            MP_LOGI("APP", "Uptime: %llu ms, Current thread: %s (ID: %d)",
                   (unsigned long long)uptime_ms, current->name, current->id);
        } else {
            MP_LOGI("APP", "Uptime: %llu ms", (unsigned long long)uptime_ms);
        }
        
        // Wait for 2 seconds
        mp_thread_sleep(2000);
    }
    
    MP_LOGI("APP", "Monitor task exiting");
    mp_thread_exit(NULL);
}

/**
 * @brief Create all application tasks
 */
int app_blink_create_tasks(void) {
    MP_LOGI("APP", "Creating application tasks");
    
    // Create blink task
    mp_thread_attr_t blink_attr = {
        .name = "blink",
        .priority = BLINK_TASK_PRIORITY,
        .stack_size = 2048
    };
    
    mp_thread_id_t blink_tid = mp_thread_create(blink_task, NULL, &blink_attr);
    if (blink_tid == 0) {
        MP_LOGE("APP", "Failed to create blink task");
        return -1;
    }
    MP_LOGI("APP", "Blink task created with ID: %d", blink_tid);
    
    // Create monitor task
    mp_thread_attr_t monitor_attr = {
        .name = "monitor",
        .priority = MONITOR_TASK_PRIORITY,
        .stack_size = 2048
    };
    
    mp_thread_id_t monitor_tid = mp_thread_create(monitor_task, NULL, &monitor_attr);
    if (monitor_tid == 0) {
        MP_LOGE("APP", "Failed to create monitor task");
        return -1;
    }
    MP_LOGI("APP", "Monitor task created with ID: %d", monitor_tid);
    
    return 0;
}

/**
 * @brief Stop the application
 */
void app_blink_stop(void) {
    MP_LOGI("APP", "Stopping application");
    app_running = false;
}

/**
 * @brief Main application entry point
 * 
 * This function initializes the application and creates all tasks.
 */
int app_blink_main(void) {
    // Initialize application
    if (app_blink_init() != 0) {
        MP_LOGE("APP", "Application initialization failed");
        return -1;
    }
    
    // Create tasks
    if (app_blink_create_tasks() != 0) {
        MP_LOGE("APP", "Failed to create application tasks");
        return -1;
    }
    
    MP_LOGI("APP", "Application started successfully");
    
    return 0;
}
