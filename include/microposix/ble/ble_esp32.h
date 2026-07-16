#ifndef MICROPOSIX_BLE_ESP32_H
#define MICROPOSIX_BLE_ESP32_H

#include <stdint.h>
#include <stdbool.h>
#include "microposix/ble/ble_mgr.h"
#include "microposix/kernel/mq.h"

// Initialize BLE backend for ESP32
int mp_ble_esp32_init(void);

// Deinitialize BLE backend
void mp_ble_esp32_deinit(void);

// Start BLE advertising
void mp_ble_esp32_start_adv(void);

// Stop BLE advertising
void mp_ble_esp32_stop_adv(void);

// Get current BLE state
mp_ble_state_t mp_ble_esp32_get_state(void);

// Set connection parameters
void mp_ble_esp32_set_conn_params(mp_ble_conn_params_t *params);

// Get connection parameters
void mp_ble_esp32_get_conn_params(mp_ble_conn_params_t *params);

// Send data over BLE
int mp_ble_esp32_send_data(uint16_t conn_handle, uint8_t *data, uint16_t len);

// Receive data from BLE (non-blocking)
int mp_ble_esp32_receive_data(uint8_t *data, uint16_t max_len, uint32_t timeout_ms);

// Get BLE timing oracle for tickless idle
uint32_t mp_ble_esp32_get_next_anchor_ticks(void);

// BLE host task
void *mp_ble_esp32_host_task(void *arg);

// Get BLE RX message queue
mp_mq_t *mp_ble_esp32_get_rx_mq(void);

// Get BLE TX message queue
mp_mq_t *mp_ble_esp32_get_tx_mq(void);

// Get BLE event message queue
mp_mq_t *mp_ble_esp32_get_event_mq(void);

// BLE backend configuration
#define MP_BLE_ESP32_MAX_CONNECTIONS 4
#define MP_BLE_ESP32_MTU 247
#define MP_BLE_ESP32_RX_QUEUE_SIZE 32
#define MP_BLE_ESP32_TX_QUEUE_SIZE 32

// Check if BLE is supported
#ifdef CONFIG_BT_ENABLE
#define MP_BLE_ESP32_SUPPORTED 1
#else
#define MP_BLE_ESP32_SUPPORTED 0
#endif

#endif // MICROPOSIX_BLE_ESP32_H
