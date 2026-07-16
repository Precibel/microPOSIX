/**
 * ESP32 GPIO HAL Implementation
 * 
 * This file provides GPIO functions for ESP32.
 */

#include <stdint.h>
#include <stdbool.h>
#include "microposix/hal/esp32/gpio.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "mp_gpio";

// Initialize GPIO pin
int mp_hal_esp32_gpio_init(gpio_num_t pin, mp_hal_esp32_gpio_dir_t dir, 
                           mp_hal_esp32_gpio_pull_t pull) {
    if (pin >= GPIO_NUM_MAX) {
        ESP_LOGE(TAG, "Invalid GPIO pin: %d", pin);
        return -1;
    }
    
    // Configure GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << pin),
        .mode = GPIO_MODE_DISABLE,
        .pull_up_en = false,
        .pull_down_en = false,
        .intr_type = GPIO_INTR_DISABLE,
    };
    
    // Set direction
    switch (dir) {
        case MP_HAL_ESP32_GPIO_INPUT:
            io_conf.mode = GPIO_MODE_INPUT;
            break;
        case MP_HAL_ESP32_GPIO_OUTPUT:
            io_conf.mode = GPIO_MODE_OUTPUT;
            break;
        case MP_HAL_ESP32_GPIO_INPUT_OUTPUT:
            io_conf.mode = GPIO_MODE_INPUT_OUTPUT;
            break;
        default:
            io_conf.mode = GPIO_MODE_DISABLE;
            break;
    }
    
    // Set pull
    switch (pull) {
        case MP_HAL_ESP32_GPIO_PULLUP:
            io_conf.pull_up_en = true;
            break;
        case MP_HAL_ESP32_GPIO_PULLDOWN:
            io_conf.pull_down_en = true;
            break;
        default:
            break;
    }
    
    // Configure GPIO
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO %d: %s", pin, esp_err_to_name(err));
        return -1;
    }
    
    ESP_LOGD(TAG, "GPIO %d initialized as %s with %s", 
             pin, 
             dir == MP_HAL_ESP32_GPIO_INPUT ? "INPUT" : 
             (dir == MP_HAL_ESP32_GPIO_OUTPUT ? "OUTPUT" : "INPUT_OUTPUT"),
             pull == MP_HAL_ESP32_GPIO_PULLUP ? "PULLUP" :
             (pull == MP_HAL_ESP32_GPIO_PULLDOWN ? "PULLDOWN" : "NOPULL"));
    
    return 0;
}

// Deinitialize GPIO pin
void mp_hal_esp32_gpio_deinit(gpio_num_t pin) {
    if (pin >= GPIO_NUM_MAX) {
        return;
    }
    
    // Reset GPIO to default state
    gpio_reset_pin(pin);
    ESP_LOGD(TAG, "GPIO %d deinitialized", pin);
}

// Set GPIO output level
void mp_hal_esp32_gpio_write(gpio_num_t pin, uint32_t level) {
    if (pin >= GPIO_NUM_MAX) {
        return;
    }
    
    gpio_set_level(pin, level ? 1 : 0);
}

// Read GPIO input level
uint32_t mp_hal_esp32_gpio_read(gpio_num_t pin) {
    if (pin >= GPIO_NUM_MAX) {
        return 0;
    }
    
    return gpio_get_level(pin);
}

// Toggle GPIO output
void mp_hal_esp32_gpio_toggle(gpio_num_t pin) {
    if (pin >= GPIO_NUM_MAX) {
        return;
    }
    
    uint32_t current = gpio_get_level(pin);
    gpio_set_level(pin, current ? 0 : 1);
}

// Set GPIO interrupt
int mp_hal_esp32_gpio_set_intr(gpio_num_t pin, mp_hal_esp32_gpio_intr_t intr_type) {
    if (pin >= GPIO_NUM_MAX) {
        return -1;
    }
    
    gpio_intr_type_t esp_intr_type;
    switch (intr_type) {
        case MP_HAL_ESP32_GPIO_INTR_DISABLE:
            esp_intr_type = GPIO_INTR_DISABLE;
            break;
        case MP_HAL_ESP32_GPIO_INTR_POSEDGE:
            esp_intr_type = GPIO_INTR_POSEDGE;
            break;
        case MP_HAL_ESP32_GPIO_INTR_NEGEDGE:
            esp_intr_type = GPIO_INTR_NEGEDGE;
            break;
        case MP_HAL_ESP32_GPIO_INTR_ANYEDGE:
            esp_intr_type = GPIO_INTR_ANYEDGE;
            break;
        case MP_HAL_ESP32_GPIO_INTR_LOWLEVEL:
            esp_intr_type = GPIO_INTR_LOW_LEVEL;
            break;
        case MP_HAL_ESP32_GPIO_INTR_HIGHLEVEL:
            esp_intr_type = GPIO_INTR_HIGH_LEVEL;
            break;
        default:
            esp_intr_type = GPIO_INTR_DISABLE;
            break;
    }
    
    esp_err_t err = gpio_set_intr_type(pin, esp_intr_type);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set interrupt type for GPIO %d: %s", pin, esp_err_to_name(err));
        return -1;
    }
    
    return 0;
}

// Clear GPIO interrupt
void mp_hal_esp32_gpio_clear_intr(gpio_num_t pin) {
    if (pin >= GPIO_NUM_MAX) {
        return;
    }
    
    // Clear interrupt status
    gpio_intr_clear(pin);
}

// GPIO interrupt callback structure
typedef struct {
    void (*callback)(void*);
    void *arg;
} mp_hal_esp32_gpio_callback_t;

// GPIO callback table
static mp_hal_esp32_gpio_callback_t gpio_callbacks[GPIO_NUM_MAX] = {0};

// GPIO ISR handler
static void IRAM_ATTR mp_hal_esp32_gpio_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t)arg;
    
    if (gpio_num >= GPIO_NUM_MAX) {
        return;
    }
    
    // Clear interrupt
    gpio_intr_clear(gpio_num);
    
    // Call callback if registered
    if (gpio_callbacks[gpio_num].callback != NULL) {
        gpio_callbacks[gpio_num].callback(gpio_callbacks[gpio_num].arg);
    }
}

// Register GPIO interrupt callback
int mp_hal_esp32_gpio_register_callback(gpio_num_t pin, void (*callback)(void*), void *arg) {
    if (pin >= GPIO_NUM_MAX) {
        return -1;
    }
    
    // Store callback
    gpio_callbacks[pin].callback = callback;
    gpio_callbacks[pin].arg = arg;
    
    // Install ISR service
    static bool isr_service_installed = false;
    if (!isr_service_installed) {
        esp_err_t err = gpio_install_isr_service(0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to install GPIO ISR service: %s", esp_err_to_name(err));
            return -1;
        }
        isr_service_installed = true;
    }
    
    // Add ISR handler for this pin
    esp_err_t err = gpio_isr_handler_add(pin, mp_hal_esp32_gpio_isr_handler, (void *)pin);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler for GPIO %d: %s", pin, esp_err_to_name(err));
        return -1;
    }
    
    return 0;
}

// Unregister GPIO interrupt callback
void mp_hal_esp32_gpio_unregister_callback(gpio_num_t pin) {
    if (pin >= GPIO_NUM_MAX) {
        return;
    }
    
    // Remove ISR handler
    gpio_isr_handler_remove(pin);
    
    // Clear callback
    gpio_callbacks[pin].callback = NULL;
    gpio_callbacks[pin].arg = NULL;
}

// Set GPIO as open-drain
void mp_hal_esp32_gpio_set_opendrain(gpio_num_t pin, bool enable) {
    if (pin >= GPIO_NUM_MAX) {
        return;
    }
    
    gpio_set_open_drain(pin, enable);
}

// Set GPIO drive strength
void mp_hal_esp32_gpio_set_drive_strength(gpio_num_t pin, uint32_t strength) {
    if (pin >= GPIO_NUM_MAX) {
        return;
    }
    
    // ESP32 GPIO drive strength: 0-3 (0 = weakest, 3 = strongest)
    gpio_set_drive_capability(pin, (gpio_drive_cap_t)strength);
}

// Set GPIO as input with glitch filter
void mp_hal_esp32_gpio_set_glitch_filter(gpio_num_t pin, bool enable) {
    if (pin >= GPIO_NUM_MAX) {
        return;
    }
    
    gpio_set_glitch_filter(pin, enable);
}
