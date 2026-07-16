/**
 * microPOSIX ESP32 Demo - Main Application
 * 
 * This is the main entry point for the ESP32 demo application.
 * It initializes the microPOSIX kernel and starts the blinking LED demo.
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "driver/gpio.h"

#include "microposix/kernel/scheduler.h"
#include "microposix/kernel/thread.h"
#include "microposix/debug/log.h"

static const char *TAG = "mp_main";

// Forward declaration of the blink task
void blink_task(void *arg);

// Main application entry point
void app_main(void) {
    ESP_LOGI(TAG, "Starting microPOSIX ESP32 Demo");
    ESP_LOGI(TAG, "IDF version: %s", esp_get_idf_version());
    
    // Initialize microPOSIX kernel
    ESP_LOGI(TAG, "Initializing microPOSIX kernel...");
    mp_sched_init();
    
    // Initialize platform-specific HAL
    ESP_LOGI(TAG, "Initializing ESP32 HAL...");
    mp_hal_esp32_cpu_init();
    
    // Initialize logging
    ESP_LOGI(TAG, "Initializing logging...");
    // Note: In a real implementation, we'd initialize the UART for logging
    
    // Create the blink task using microPOSIX API
    ESP_LOGI(TAG, "Creating blink task...");
    mp_thread_attr_t blink_attr = {
        .name = "blink",
        .priority = 10,
        .stack_size = 2048
    };
    
    mp_thread_id_t blink_tid = mp_thread_create(blink_task, NULL, &blink_attr);
    if (blink_tid == 0) {
        ESP_LOGE(TAG, "Failed to create blink task");
        return;
    }
    
    ESP_LOGI(TAG, "Blink task created with ID: %d", blink_tid);
    
    // Start the scheduler
    ESP_LOGI(TAG, "Starting scheduler...");
    mp_sched_start();
    
    // The scheduler should now be running
    // On ESP32 with FreeRTOS, this will run as a FreeRTOS task
    
    // Main loop - should never reach here in a proper RTOS
    while (1) {
        ESP_LOGI(TAG, "Main loop running...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// Blink task implementation
void blink_task(void *arg) {
    (void)arg;
    
    ESP_LOGI(TAG, "Blink task started");
    
    // Initialize GPIO for the built-in LED
    // Most ESP32 dev boards have a built-in LED on GPIO 2
    gpio_num_t led_pin = MP_HAL_ESP32_GPIO_LED_BUILTIN;
    
    ESP_LOGI(TAG, "Initializing LED on GPIO %d", led_pin);
    
    // Configure LED GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << led_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure LED GPIO: %s", esp_err_to_name(err));
        return;
    }
    
    // Blink loop
    int count = 0;
    while (1) {
        // Turn LED on
        gpio_set_level(led_pin, 1);
        ESP_LOGI(TAG, "LED ON (count: %d)", count++);
        
        // Wait for 500ms
        mp_thread_sleep(500);
        
        // Turn LED off
        gpio_set_level(led_pin, 0);
        ESP_LOGI(TAG, "LED OFF");
        
        // Wait for 500ms
        mp_thread_sleep(500);
    }
}
