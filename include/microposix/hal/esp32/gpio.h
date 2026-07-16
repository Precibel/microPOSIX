#ifndef MICROPOSIX_HAL_ESP32_GPIO_H
#define MICROPOSIX_HAL_ESP32_GPIO_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"

// GPIO direction
typedef enum {
    MP_HAL_ESP32_GPIO_INPUT = 0,
    MP_HAL_ESP32_GPIO_OUTPUT = 1,
    MP_HAL_ESP32_GPIO_INPUT_OUTPUT = 2
} mp_hal_esp32_gpio_dir_t;

// GPIO pull mode
typedef enum {
    MP_HAL_ESP32_GPIO_NOPULL = 0,
    MP_HAL_ESP32_GPIO_PULLUP = 1,
    MP_HAL_ESP32_GPIO_PULLDOWN = 2
} mp_hal_esp32_gpio_pull_t;

// GPIO interrupt type
typedef enum {
    MP_HAL_ESP32_GPIO_INTR_DISABLE = 0,
    MP_HAL_ESP32_GPIO_INTR_POSEDGE = 1,
    MP_HAL_ESP32_GPIO_INTR_NEGEDGE = 2,
    MP_HAL_ESP32_GPIO_INTR_ANYEDGE = 3,
    MP_HAL_ESP32_GPIO_INTR_LOWLEVEL = 4,
    MP_HAL_ESP32_GPIO_INTR_HIGHLEVEL = 5
} mp_hal_esp32_gpio_intr_t;

// Initialize GPIO pin
int mp_hal_esp32_gpio_init(gpio_num_t pin, mp_hal_esp32_gpio_dir_t dir, 
                           mp_hal_esp32_gpio_pull_t pull);

// Deinitialize GPIO pin
void mp_hal_esp32_gpio_deinit(gpio_num_t pin);

// Set GPIO output level
void mp_hal_esp32_gpio_write(gpio_num_t pin, uint32_t level);

// Read GPIO input level
uint32_t mp_hal_esp32_gpio_read(gpio_num_t pin);

// Toggle GPIO output
void mp_hal_esp32_gpio_toggle(gpio_num_t pin);

// Set GPIO interrupt
int mp_hal_esp32_gpio_set_intr(gpio_num_t pin, mp_hal_esp32_gpio_intr_t intr_type);

// Clear GPIO interrupt
void mp_hal_esp32_gpio_clear_intr(gpio_num_t pin);

// Register GPIO interrupt callback
int mp_hal_esp32_gpio_register_callback(gpio_num_t pin, void (*callback)(void*), void *arg);

// Unregister GPIO interrupt callback
void mp_hal_esp32_gpio_unregister_callback(gpio_num_t pin);

// ESP32-specific: Set GPIO as open-drain
void mp_hal_esp32_gpio_set_opendrain(gpio_num_t pin, bool enable);

// ESP32-specific: Set GPIO drive strength
void mp_hal_esp32_gpio_set_drive_strength(gpio_num_t pin, uint32_t strength);

// ESP32-specific: Set GPIO as input with glitch filter
void mp_hal_esp32_gpio_set_glitch_filter(gpio_num_t pin, bool enable);

// Common LED GPIO pins for ESP32 dev boards
#if defined(CONFIG_IDF_TARGET_ESP32)
#define MP_HAL_ESP32_GPIO_LED_BUILTIN GPIO_NUM_2  // Most ESP32 dev boards
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define MP_HAL_ESP32_GPIO_LED_BUILTIN GPIO_NUM_8  // ESP32-S3 dev board
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define MP_HAL_ESP32_GPIO_LED_BUILTIN GPIO_NUM_8  // ESP32-C3 dev board
#else
#define MP_HAL_ESP32_GPIO_LED_BUILTIN GPIO_NUM_2
#endif

#endif // MICROPOSIX_HAL_ESP32_GPIO_H
