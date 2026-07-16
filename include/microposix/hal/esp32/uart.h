#ifndef MICROPOSIX_HAL_ESP32_UART_H
#define MICROPOSIX_HAL_ESP32_UART_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/uart.h"
#include "driver/gpio.h"

// UART port definitions for ESP32
// ESP32 has 3 UART ports: UART0, UART1, UART2
// UART0 is used for programming and debugging
// UART1 and UART2 are available for application use

typedef enum {
    MP_HAL_ESP32_UART_0 = 0,  // Typically used for programming
    MP_HAL_ESP32_UART_1 = 1,  // Available for application
    MP_HAL_ESP32_UART_2 = 2   // Available for application
} mp_hal_esp32_uart_port_t;

// Default UART configuration for microPOSIX shell
#define MP_HAL_ESP32_UART_SHELL_PORT MP_HAL_ESP32_UART_0
#define MP_HAL_ESP32_UART_SHELL_BAUD 115200  // Can be increased to 921600
#define MP_HAL_ESP32_UART_SHELL_TXD GPIO_NUM_1
#define MP_HAL_ESP32_UART_SHELL_RXD GPIO_NUM_3

// UART buffer sizes
#define MP_HAL_ESP32_UART_TX_BUFFER_SIZE 4096
#define MP_HAL_ESP32_UART_RX_BUFFER_SIZE 512

// Initialize UART
int mp_hal_esp32_uart_init(mp_hal_esp32_uart_port_t port, uint32_t baud_rate, 
                           gpio_num_t tx_pin, gpio_num_t rx_pin);

// Deinitialize UART
void mp_hal_esp32_uart_deinit(mp_hal_esp32_uart_port_t port);

// Send single byte (blocking)
int mp_hal_esp32_uart_send_byte(mp_hal_esp32_uart_port_t port, uint8_t byte);

// Send data (blocking)
int mp_hal_esp32_uart_send_data(mp_hal_esp32_uart_port_t port, const uint8_t *data, size_t len);

// Receive single byte (non-blocking)
int mp_hal_esp32_uart_receive_byte(mp_hal_esp32_uart_port_t port, uint8_t *byte);

// Receive data (non-blocking)
int mp_hal_esp32_uart_receive_data(mp_hal_esp32_uart_port_t port, uint8_t *data, size_t max_len);

// Check if data is available
bool mp_hal_esp32_uart_available(mp_hal_esp32_uart_port_t port);

// Flush UART transmitter
void mp_hal_esp32_uart_flush(mp_hal_esp32_uart_port_t port);

// Enable UART DMA (if supported)
// Note: ESP32 UART doesn't have true DMA, but we can use I2S for DMA-like behavior
// For simplicity, we'll use interrupt-driven UART with ring buffers

// Set up UART with ring buffers for non-blocking operation
int mp_hal_esp32_uart_setup_ringbuffer(mp_hal_esp32_uart_port_t port, 
                                       size_t tx_buffer_size, size_t rx_buffer_size);

// UART ISR handler (to be called from FreeRTOS task)
void mp_hal_esp32_uart_isr_handler(void *arg);

#endif // MICROPOSIX_HAL_ESP32_UART_H
