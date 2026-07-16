/**
 * ESP32 UART HAL Implementation
 * 
 * This file provides UART functions for ESP32.
 */

#include <stdint.h>
#include <stdbool.h>
#include "microposix/hal/esp32/uart.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "mp_uart";

// UART configuration structure
typedef struct {
    uart_port_t port;
    QueueHandle_t tx_queue;
    QueueHandle_t rx_queue;
    bool initialized;
} mp_hal_esp32_uart_config_t;

// UART configurations for all ports
static mp_hal_esp32_uart_config_t uart_configs[3] = {0};  // UART0, UART1, UART2

// Initialize UART
int mp_hal_esp32_uart_init(mp_hal_esp32_uart_port_t port_num, uint32_t baud_rate, 
                           gpio_num_t tx_pin, gpio_num_t rx_pin) {
    if (port_num >= 3) {
        ESP_LOGE(TAG, "Invalid UART port: %d", port_num);
        return -1;
    }
    
    uart_port_t uart_port = (uart_port_t)port_num;
    mp_hal_esp32_uart_config_t *config = &uart_configs[port_num];
    
    if (config->initialized) {
        ESP_LOGW(TAG, "UART %d already initialized", port_num);
        return 0;
    }
    
    // Configure UART
    uart_config_t uart_config = {
        .baud_rate = baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    
    // Install UART driver
    esp_err_t err = uart_driver_install(uart_port, MP_HAL_ESP32_UART_TX_BUFFER_SIZE, 
                                         MP_HAL_ESP32_UART_RX_BUFFER_SIZE, 0, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART %d driver: %s", port_num, esp_err_to_name(err));
        return -1;
    }
    
    // Configure UART parameters
    err = uart_param_config(uart_port, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART %d parameters: %s", port_num, esp_err_to_name(err));
        uart_driver_delete(uart_port);
        return -1;
    }
    
    // Set UART pins
    err = uart_set_pin(uart_port, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART %d pins: %s", port_num, esp_err_to_name(err));
        uart_driver_delete(uart_port);
        return -1;
    }
    
    // Create queues for non-blocking operation
    config->tx_queue = xQueueCreate(MP_HAL_ESP32_UART_TX_BUFFER_SIZE, sizeof(uint8_t));
    config->rx_queue = xQueueCreate(MP_HAL_ESP32_UART_RX_BUFFER_SIZE, sizeof(uint8_t));
    
    if (config->tx_queue == NULL || config->rx_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create UART %d queues", port_num);
        uart_driver_delete(uart_port);
        return -1;
    }
    
    config->port = uart_port;
    config->initialized = true;
    
    ESP_LOGI(TAG, "UART %d initialized with baud rate %d, TX: %d, RX: %d", 
             port_num, baud_rate, tx_pin, rx_pin);
    
    return 0;
}

// Deinitialize UART
void mp_hal_esp32_uart_deinit(mp_hal_esp32_uart_port_t port_num) {
    if (port_num >= 3) {
        return;
    }
    
    mp_hal_esp32_uart_config_t *config = &uart_configs[port_num];
    
    if (!config->initialized) {
        return;
    }
    
    // Delete queues
    if (config->tx_queue != NULL) {
        vQueueDelete(config->tx_queue);
        config->tx_queue = NULL;
    }
    
    if (config->rx_queue != NULL) {
        vQueueDelete(config->rx_queue);
        config->rx_queue = NULL;
    }
    
    // Delete UART driver
    uart_driver_delete(config->port);
    config->initialized = false;
    
    ESP_LOGI(TAG, "UART %d deinitialized", port_num);
}

// Send single byte (blocking)
int mp_hal_esp32_uart_send_byte(mp_hal_esp32_uart_port_t port_num, uint8_t byte) {
    if (port_num >= 3) {
        return -1;
    }
    
    mp_hal_esp32_uart_config_t *config = &uart_configs[port_num];
    
    if (!config->initialized) {
        return -1;
    }
    
    // Send byte directly (blocking)
    int bytes_written = uart_write_bytes(config->port, &byte, 1);
    if (bytes_written < 0) {
        return -1;
    }
    
    return 0;
}

// Send data (blocking)
int mp_hal_esp32_uart_send_data(mp_hal_esp32_uart_port_t port_num, const uint8_t *data, size_t len) {
    if (port_num >= 3) {
        return -1;
    }
    
    mp_hal_esp32_uart_config_t *config = &uart_configs[port_num];
    
    if (!config->initialized) {
        return -1;
    }
    
    // Send data directly (blocking)
    int bytes_written = uart_write_bytes(config->port, data, len);
    if (bytes_written < 0) {
        return -1;
    }
    
    return bytes_written;
}

// Receive single byte (non-blocking)
int mp_hal_esp32_uart_receive_byte(mp_hal_esp32_uart_port_t port_num, uint8_t *byte) {
    if (port_num >= 3) {
        return -1;
    }
    
    mp_hal_esp32_uart_config_t *config = &uart_configs[port_num];
    
    if (!config->initialized) {
        return -1;
    }
    
    // Try to read from UART
    int bytes_read = uart_read_bytes(config->port, byte, 1, 0);
    if (bytes_read > 0) {
        return 0;
    }
    
    return -1;  // No data available
}

// Receive data (non-blocking)
int mp_hal_esp32_uart_receive_data(mp_hal_esp32_uart_port_t port_num, uint8_t *data, size_t max_len) {
    if (port_num >= 3) {
        return -1;
    }
    
    mp_hal_esp32_uart_config_t *config = &uart_configs[port_num];
    
    if (!config->initialized) {
        return -1;
    }
    
    // Try to read from UART
    int bytes_read = uart_read_bytes(config->port, data, max_len, 0);
    if (bytes_read > 0) {
        return bytes_read;
    }
    
    return 0;  // No data available
}

// Check if data is available
bool mp_hal_esp32_uart_available(mp_hal_esp32_uart_port_t port_num) {
    if (port_num >= 3) {
        return false;
    }
    
    mp_hal_esp32_uart_config_t *config = &uart_configs[port_num];
    
    if (!config->initialized) {
        return false;
    }
    
    size_t available;
    esp_err_t err = uart_get_buffered_data_len(config->port, &available);
    if (err != ESP_OK) {
        return false;
    }
    
    return available > 0;
}

// Flush UART transmitter
void mp_hal_esp32_uart_flush(mp_hal_esp32_uart_port_t port_num) {
    if (port_num >= 3) {
        return;
    }
    
    mp_hal_esp32_uart_config_t *config = &uart_configs[port_num];
    
    if (!config->initialized) {
        return;
    }
    
    uart_wait_tx_done(config->port, pdMS_TO_TICKS(100));
}

// UART event handler (for interrupt-driven operation)
static void mp_hal_esp32_uart_event_handler(void *arg) {
    uart_port_t uart_port = (uart_port_t)arg;
    uint8_t data[128];
    uart_event_t event;
    
    while (1) {
        // Wait for UART event
        if (xQueueReceive(uart_configs[uart_port].rx_queue, &event, portMAX_DELAY) == pdTRUE) {
            switch (event.type) {
                case UART_DATA:
                    // Read data from UART
                    int bytes_read = uart_read_bytes(uart_port, data, event.size, 0);
                    if (bytes_read > 0) {
                        // Process received data
                        // In a real implementation, we'd buffer this or pass to shell
                    }
                    break;
                
                case UART_FIFO_OVF:
                    ESP_LOGE(TAG, "UART %d FIFO overflow", uart_port);
                    uart_flush_input(uart_port);
                    break;
                
                case UART_BUFFER_FULL:
                    ESP_LOGW(TAG, "UART %d buffer full", uart_port);
                    uart_flush_input(uart_port);
                    break;
                
                default:
                    break;
            }
        }
    }
}

// Set up UART with ring buffers for non-blocking operation
int mp_hal_esp32_uart_setup_ringbuffer(mp_hal_esp32_uart_port_t port_num, 
                                       size_t tx_buffer_size, size_t rx_buffer_size) {
    if (port_num >= 3) {
        return -1;
    }
    
    mp_hal_esp32_uart_config_t *config = &uart_configs[port_num];
    
    if (!config->initialized) {
        // Initialize UART first
        if (port_num == MP_HAL_ESP32_UART_0) {
            mp_hal_esp32_uart_init(port_num, MP_HAL_ESP32_UART_SHELL_BAUD, 
                                  MP_HAL_ESP32_UART_SHELL_TXD, MP_HAL_ESP32_UART_SHELL_RXD);
        } else {
            // Use default pins for other UARTs
            mp_hal_esp32_uart_init(port_num, 115200, GPIO_NUM_17, GPIO_NUM_16);
        }
    }
    
    // Recreate queues with specified sizes
    if (config->tx_queue != NULL) {
        vQueueDelete(config->tx_queue);
    }
    if (config->rx_queue != NULL) {
        vQueueDelete(config->rx_queue);
    }
    
    config->tx_queue = xQueueCreate(tx_buffer_size, sizeof(uint8_t));
    config->rx_queue = xQueueCreate(rx_buffer_size, sizeof(uint8_t));
    
    if (config->tx_queue == NULL || config->rx_queue == NULL) {
        return -1;
    }
    
    // Set up event queue for UART
    uart_event_queue_t event_queue;
    esp_err_t err = uart_enable_rx_intr(config->port, tx_buffer_size);
    if (err != ESP_OK) {
        return -1;
    }
    
    // Create event queue
    event_queue = xQueueCreate(10, sizeof(uart_event_t));
    if (event_queue == NULL) {
        return -1;
    }
    
    // Register event queue
    err = uart_isr_register(config->port, mp_hal_esp32_uart_event_handler, (void *)config->port);
    if (err != ESP_OK) {
        return -1;
    }
    
    return 0;
}

// UART ISR handler (to be called from FreeRTOS task)
void mp_hal_esp32_uart_isr_handler(void *arg) {
    // This is a placeholder for the ISR handler
    // In practice, we use the event handler above
}
