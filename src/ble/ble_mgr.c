/**
 * microPOSIX BLE Manager
 * 
 * This file provides a platform-agnostic BLE manager interface.
 * It abstracts the underlying BLE stack (ESP-IDF, Nordic SoftDevice, NimBLE, etc.)
 * and provides a consistent API for applications.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "microposix/ble/ble_mgr.h"
#include "microposix/kernel/mq.h"
#include "microposix/debug/log.h"
#include "microposix/kernel/thread.h"

// Platform detection
#if defined(MICROPOSIX_PLATFORM_ESP32)
#include "microposix/ble/ble_esp32.h"
#define BLE_BACKEND_ESP32 1
#elif defined(MICROPOSIX_PLATFORM_ARM)
// #include "microposix/ble/ble_nordic.h"
// #define BLE_BACKEND_NORDIC 1
#elif defined(MICROPOSIX_PLATFORM_RISCV)
// #include "microposix/ble/ble_nimble.h"
// #define BLE_BACKEND_NIMBLE 1
#endif

// BLE state
static mp_ble_state_t ble_state = MP_BLE_DISCONNECTED;

// Connection parameters
static mp_ble_conn_params_t conn_params = {
    .min_interval = MP_BLE_CONN_INTERVAL_MIN,
    .max_interval = MP_BLE_CONN_INTERVAL_MAX,
    .latency = 0,
    .supervision_timeout = 1000  // 1000ms
};

// BLE event callback
static mp_ble_event_callback_t ble_event_callback = NULL;

// BLE data callback
static mp_ble_data_callback_t ble_data_callback = NULL;

// BLE backend function pointers (for dynamic backend selection)
static struct {
    int (*init)(void);
    void (*deinit)(void);
    void (*start_adv)(void);
    void (*stop_adv)(void);
    mp_ble_state_t (*get_state)(void);
    void (*set_conn_params)(mp_ble_conn_params_t *);
    void (*get_conn_params)(mp_ble_conn_params_t *);
    int (*send_data)(uint16_t, uint8_t *, uint16_t);
    int (*receive_data)(uint8_t *, uint16_t, uint32_t);
    uint32_t (*get_next_anchor_ticks)(void);
    void *(*host_task)(void *);
    mp_mq_t *(*get_rx_mq)(void);
    mp_mq_t *(*get_tx_mq)(void);
    mp_mq_t *(*get_event_mq)(void);
} ble_backend = {0};

/**
 * @brief Initialize BLE manager
 */
void mp_ble_init(void) {
    MP_LOGI("BLE", "Initializing BLE Manager");
    
    // Select backend based on platform
    #if defined(BLE_BACKEND_ESP32)
    MP_LOGI("BLE", "Using ESP32 backend");
    ble_backend.init = mp_ble_esp32_init;
    ble_backend.deinit = mp_ble_esp32_deinit;
    ble_backend.start_adv = mp_ble_esp32_start_adv;
    ble_backend.stop_adv = mp_ble_esp32_stop_adv;
    ble_backend.get_state = mp_ble_esp32_get_state;
    ble_backend.set_conn_params = mp_ble_esp32_set_conn_params;
    ble_backend.get_conn_params = mp_ble_esp32_get_conn_params;
    ble_backend.send_data = mp_ble_esp32_send_data;
    ble_backend.receive_data = mp_ble_esp32_receive_data;
    ble_backend.get_next_anchor_ticks = mp_ble_esp32_get_next_anchor_ticks;
    ble_backend.host_task = mp_ble_esp32_host_task;
    ble_backend.get_rx_mq = mp_ble_esp32_get_rx_mq;
    ble_backend.get_tx_mq = mp_ble_esp32_get_tx_mq;
    ble_backend.get_event_mq = mp_ble_esp32_get_event_mq;
    
    #elif defined(BLE_BACKEND_NORDIC)
    MP_LOGI("BLE", "Using Nordic SoftDevice backend");
    // ble_backend.init = mp_ble_nordic_init;
    // ... other Nordic functions
    
    #elif defined(BLE_BACKEND_NIMBLE)
    MP_LOGI("BLE", "Using NimBLE backend");
    // ble_backend.init = mp_ble_nimble_init;
    // ... other NimBLE functions
    
    #else
    MP_LOGW("BLE", "No BLE backend selected, using simulation");
    // Use simulation backend for testing
    ble_backend.get_state = mp_ble_sim_get_state;
    ble_backend.start_adv = mp_ble_sim_start_adv;
    ble_backend.stop_adv = mp_ble_sim_stop_adv;
    #endif
    
    // Initialize the selected backend
    if (ble_backend.init && ble_backend.init() != 0) {
        MP_LOGE("BLE", "Failed to initialize BLE backend");
        ble_state = MP_BLE_ERROR;
        return;
    }
    
    ble_state = MP_BLE_READY;
    MP_LOGI("BLE", "BLE Manager initialized successfully");
}

/**
 * @brief Deinitialize BLE manager
 */
void mp_ble_deinit(void) {
    MP_LOGI("BLE", "Deinitializing BLE Manager");
    
    if (ble_backend.deinit) {
        ble_backend.deinit();
    }
    
    ble_state = MP_BLE_DISCONNECTED;
    MP_LOGI("BLE", "BLE Manager deinitialized");
}

/**
 * @brief Start BLE advertising
 */
void mp_ble_start_adv(void) {
    if (ble_state != MP_BLE_READY && ble_state != MP_BLE_DISCONNECTED) {
        MP_LOGW("BLE", "Cannot start advertising, invalid state: %d", ble_state);
        return;
    }
    
    if (ble_backend.start_adv) {
        ble_backend.start_adv();
    }
    
    ble_state = MP_BLE_ADVERTISING;
    MP_LOGI("BLE", "BLE advertising started");
    
    // Notify callback
    if (ble_event_callback) {
        ble_event_callback(MP_BLE_EVENT_ADV_STARTED, 0, NULL, 0);
    }
}

/**
 * @brief Stop BLE advertising
 */
void mp_ble_stop_adv(void) {
    if (ble_state != MP_BLE_ADVERTISING) {
        MP_LOGW("BLE", "Cannot stop advertising, invalid state: %d", ble_state);
        return;
    }
    
    if (ble_backend.stop_adv) {
        ble_backend.stop_adv();
    }
    
    ble_state = MP_BLE_READY;
    MP_LOGI("BLE", "BLE advertising stopped");
    
    // Notify callback
    if (ble_event_callback) {
        ble_event_callback(MP_BLE_EVENT_ADV_STOPPED, 0, NULL, 0);
    }
}

/**
 * @brief Get current BLE state
 */
mp_ble_state_t mp_ble_get_state(void) {
    if (ble_backend.get_state) {
        return ble_backend.get_state();
    }
    return ble_state;
}

/**
 * @brief Set connection parameters
 */
void mp_ble_set_conn_params(mp_ble_conn_params_t *params) {
    if (params == NULL) {
        return;
    }
    
    memcpy(&conn_params, params, sizeof(mp_ble_conn_params_t));
    
    if (ble_backend.set_conn_params) {
        ble_backend.set_conn_params(params);
    }
    
    MP_LOGI("BLE", "Connection parameters updated");
}

/**
 * @brief Get connection parameters
 */
void mp_ble_get_conn_params(mp_ble_conn_params_t *params) {
    if (params == NULL) {
        return;
    }
    
    if (ble_backend.get_conn_params) {
        ble_backend.get_conn_params(params);
    } else {
        memcpy(params, &conn_params, sizeof(mp_ble_conn_params_t));
    }
}

/**
 * @brief Send data over BLE
 */
int mp_ble_send_data(uint16_t conn_handle, uint8_t *data, uint16_t len) {
    if (ble_state != MP_BLE_CONNECTED) {
        MP_LOGE("BLE", "Cannot send data, not connected");
        return -1;
    }
    
    if (data == NULL || len == 0) {
        MP_LOGE("BLE", "Invalid data or length");
        return -1;
    }
    
    if (ble_backend.send_data) {
        int result = ble_backend.send_data(conn_handle, data, len);
        if (result > 0 && ble_data_callback) {
            ble_data_callback(conn_handle, data, len, MP_BLE_DATA_SENT);
        }
        return result;
    }
    
    return -1;
}

/**
 * @brief Receive data from BLE (non-blocking)
 */
int mp_ble_receive_data(uint8_t *data, uint16_t max_len, uint32_t timeout_ms) {
    if (data == NULL || max_len == 0) {
        return -1;
    }
    
    if (ble_backend.receive_data) {
        return ble_backend.receive_data(data, max_len, timeout_ms);
    }
    
    return -1;
}

/**
 * @brief Get BLE timing oracle for tickless idle
 */
uint32_t mp_ble_get_next_anchor_ticks(void) {
    if (ble_backend.get_next_anchor_ticks) {
        return ble_backend.get_next_anchor_ticks();
    }
    
    // Default: no BLE activity, can sleep indefinitely
    return 0xFFFFFFFF;
}

/**
 * @brief Set BLE event callback
 */
void mp_ble_set_event_callback(mp_ble_event_callback_t callback) {
    ble_event_callback = callback;
}

/**
 * @brief Set BLE data callback
 */
void mp_ble_set_data_callback(mp_ble_data_callback_t callback) {
    ble_data_callback = callback;
}

/**
 * @brief Get BLE RX message queue
 */
mp_mq_t *mp_ble_get_rx_mq(void) {
    if (ble_backend.get_rx_mq) {
        return ble_backend.get_rx_mq();
    }
    return NULL;
}

/**
 * @brief Get BLE TX message queue
 */
mp_mq_t *mp_ble_get_tx_mq(void) {
    if (ble_backend.get_tx_mq) {
        return ble_backend.get_tx_mq();
    }
    return NULL;
}

/**
 * @brief Get BLE event message queue
 */
mp_mq_t *mp_ble_get_event_mq(void) {
    if (ble_backend.get_event_mq) {
        return ble_backend.get_event_mq();
    }
    return NULL;
}

/**
 * @brief BLE host task
 * 
 * This task handles BLE events and data processing.
 * It uses the platform-specific backend.
 */
void *mp_ble_host_task(void *arg) {
    (void)arg;
    
    MP_LOGI("BLE", "BLE host task started");
    
    // Initialize BLE
    mp_ble_init();
    
    // Start advertising
    mp_ble_start_adv();
    
    while (1) {
        // Process BLE events if backend provides event queue
        if (ble_backend.get_event_mq) {
            mp_ble_event_t event;
            if (mp_mq_receive(ble_backend.get_event_mq(), &event, 100) == 0) {
                // Handle event
                switch (event.type) {
                    case MP_BLE_EVENT_ADV_STARTED:
                        MP_LOGI("BLE", "Advertising started");
                        if (ble_event_callback) {
                            ble_event_callback(event.type, event.conn_handle, event.address, event.data_length);
                        }
                        break;
                        
                    case MP_BLE_EVENT_ADV_STOPPED:
                        MP_LOGI("BLE", "Advertising stopped");
                        if (ble_event_callback) {
                            ble_event_callback(event.type, event.conn_handle, event.address, event.data_length);
                        }
                        break;
                        
                    case MP_BLE_EVENT_CONNECTED:
                        MP_LOGI("BLE", "Device connected");
                        ble_state = MP_BLE_CONNECTED;
                        if (ble_event_callback) {
                            ble_event_callback(event.type, event.conn_handle, event.address, event.data_length);
                        }
                        break;
                        
                    case MP_BLE_EVENT_DISCONNECTED:
                        MP_LOGI("BLE", "Device disconnected");
                        ble_state = MP_BLE_DISCONNECTED;
                        if (ble_event_callback) {
                            ble_event_callback(event.type, event.conn_handle, event.address, event.data_length);
                        }
                        // Restart advertising
                        mp_ble_start_adv();
                        break;
                        
                    case MP_BLE_EVENT_DATA_RECEIVED:
                        MP_LOGD("BLE", "Data received: %d bytes", event.data_length);
                        if (ble_data_callback) {
                            // For data events, we need to read the actual data
                            uint8_t data[256];
                            int len = mp_ble_receive_data(data, sizeof(data), 0);
                            if (len > 0) {
                                ble_data_callback(event.conn_handle, data, len, MP_BLE_DATA_RECEIVED);
                            }
                        }
                        break;
                        
                    case MP_BLE_EVENT_CONN_PARAM_UPDATE:
                        MP_LOGI("BLE", "Connection parameters updated");
                        if (ble_event_callback) {
                            ble_event_callback(event.type, event.conn_handle, event.address, event.data_length);
                        }
                        break;
                        
                    default:
                        MP_LOGW("BLE", "Unknown event type: %d", event.type);
                        break;
                }
            }
        }
        
        // Yield to other tasks
        mp_thread_sleep(10);
    }
    
    return NULL;
}

// ============================================================================
// Simulation Backend (for testing without hardware)
// ============================================================================

#if !defined(BLE_BACKEND_ESP32) && !defined(BLE_BACKEND_NORDIC) && !defined(BLE_BACKEND_NIMBLE)

static mp_ble_state_t sim_ble_state = MP_BLE_DISCONNECTED;

static mp_ble_state_t mp_ble_sim_get_state(void) {
    return sim_ble_state;
}

static void mp_ble_sim_start_adv(void) {
    sim_ble_state = MP_BLE_ADVERTISING;
    MP_LOGI("BLE", "[SIM] Advertising started");
}

static void mp_ble_sim_stop_adv(void) {
    sim_ble_state = MP_BLE_READY;
    MP_LOGI("BLE", "[SIM] Advertising stopped");
}

#endif // No hardware backend
