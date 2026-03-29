#ifndef MICROPOSIX_UART_H
#define MICROPOSIX_UART_H

#include <stdint.h>
#include <stddef.h>

// UART IDs
typedef enum {
    UART0,
    UART1
} mp_uart_id_t;

// UART management functions
int mp_hal_uart_init(mp_uart_id_t id, uint32_t baud_rate);
void mp_hal_uart_send_byte(mp_uart_id_t id, uint8_t byte);
void mp_hal_uart_send_data(mp_uart_id_t id, const uint8_t *data, size_t len);
int mp_hal_uart_receive_byte(mp_uart_id_t id, uint8_t *byte);

#endif // MICROPOSIX_UART_H
