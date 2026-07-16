#ifndef MICROPOSIX_BLE_MGR_H
#define MICROPOSIX_BLE_MGR_H

#include <stdint.h>
#include <stdbool.h>
#include "microposix/kernel/mq.h"

// BLE states
typedef enum {
    MP_BLE_DISCONNECTED,    // BLE is not connected
    MP_BLE_READY,           // BLE is initialized and ready
    MP_BLE_ADVERTISING,     // BLE is advertising
    MP_BLE_CONNECTED,       // BLE is connected to a device
    MP_BLE_ERROR            // BLE error state
} mp_ble_state_t;

// BLE connection parameters
typedef struct {
    uint16_t min_interval;       // Minimum connection interval (in 1.25ms units)
    uint16_t max_interval;       // Maximum connection interval (in 1.25ms units)
    uint16_t latency;            // Slave latency (number of connection events)
    uint16_t supervision_timeout; // Supervision timeout (in 10ms units)
} mp_ble_conn_params_t;

// Default connection parameters
#define MP_BLE_CONN_INTERVAL_MIN 0x0006  // 7.5ms
#define MP_BLE_CONN_INTERVAL_MAX 0x000C  // 15ms

// BLE event types
typedef enum {
    MP_BLE_EVENT_ADV_STARTED,      // Advertising started
    MP_BLE_EVENT_ADV_STOPPED,      // Advertising stopped
    MP_BLE_EVENT_CONNECTED,        // Device connected
    MP_BLE_EVENT_DISCONNECTED,     // Device disconnected
    MP_BLE_EVENT_DATA_RECEIVED,    // Data received
    MP_BLE_EVENT_DATA_SENT,        // Data sent
    MP_BLE_EVENT_CONN_PARAM_UPDATE, // Connection parameters updated
    MP_BLE_EVENT_MTU_EXCHANGE,     // MTU exchange completed
    MP_BLE_EVENT_ERROR             // Error occurred
} mp_ble_event_type_t;

// BLE event structure
typedef struct {
    mp_ble_event_type_t type;      // Event type
    uint16_t conn_handle;          // Connection handle
    uint8_t address[6];            // Device address
    uint16_t data_length;          // Data length
    uint8_t *data;                // Data pointer (may be NULL)
} mp_ble_event_t;

// BLE data direction
typedef enum {
    MP_BLE_DATA_RECEIVED,         // Data received from peer
    MP_BLE_DATA_SENT              // Data sent to peer
} mp_ble_data_direction_t;

// BLE event callback
typedef void (*mp_ble_event_callback_t)(mp_ble_event_type_t event, uint16_t conn_handle,
                                         const uint8_t *address, uint16_t data_length);

// BLE data callback
typedef void (*mp_ble_data_callback_t)(uint16_t conn_handle, uint8_t *data, 
                                         uint16_t length, mp_ble_data_direction_t direction);

// BLE Manager Functions

/**
 * @brief Initialize BLE manager
 */
void mp_ble_init(void);

/**
 * @brief Deinitialize BLE manager
 */
void mp_ble_deinit(void);

/**
 * @brief Start BLE advertising
 */
void mp_ble_start_adv(void);

/**
 * @brief Stop BLE advertising
 */
void mp_ble_stop_adv(void);

/**
 * @brief Get current BLE state
 */
mp_ble_state_t mp_ble_get_state(void);

/**
 * @brief Set connection parameters
 */
void mp_ble_set_conn_params(mp_ble_conn_params_t *params);

/**
 * @brief Get connection parameters
 */
void mp_ble_get_conn_params(mp_ble_conn_params_t *params);

/**
 * @brief Send data over BLE
 * @param conn_handle Connection handle
 * @param data Data to send
 * @param len Length of data
 * @return Number of bytes sent, or -1 on error
 */
int mp_ble_send_data(uint16_t conn_handle, uint8_t *data, uint16_t len);

/**
 * @brief Receive data from BLE (non-blocking)
 * @param data Buffer to receive data
 * @param max_len Maximum length to receive
 * @param timeout_ms Timeout in milliseconds (0 = non-blocking)
 * @return Number of bytes received, or -1 on error
 */
int mp_ble_receive_data(uint8_t *data, uint16_t max_len, uint32_t timeout_ms);

/**
 * @brief Get BLE timing oracle for tickless idle
 * @return Number of ticks until next BLE event, or 0xFFFFFFFF if no events
 */
uint32_t mp_ble_get_next_anchor_ticks(void);

/**
 * @brief Set BLE event callback
 * @param callback Callback function for BLE events
 */
void mp_ble_set_event_callback(mp_ble_event_callback_t callback);

/**
 * @brief Set BLE data callback
 * @param callback Callback function for BLE data
 */
void mp_ble_set_data_callback(mp_ble_data_callback_t callback);

/**
 * @brief Get BLE RX message queue
 * @return Pointer to RX message queue, or NULL if not available
 */
mp_mq_t *mp_ble_get_rx_mq(void);

/**
 * @brief Get BLE TX message queue
 * @return Pointer to TX message queue, or NULL if not available
 */
mp_mq_t *mp_ble_get_tx_mq(void);

/**
 * @brief Get BLE event message queue
 * @return Pointer to event message queue, or NULL if not available
 */
mp_mq_t *mp_ble_get_event_mq(void);

/**
 * @brief BLE host task
 * @param arg Task argument (unused)
 */
void *mp_ble_host_task(void *arg);

#endif // MICROPOSIX_BLE_MGR_H
