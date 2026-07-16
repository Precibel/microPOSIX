/**
 * microPOSIX ESP32 BLE Backend
 * 
 * This file implements the BLE backend for ESP32 using ESP-IDF's Bluetooth stack.
 * It provides the interface between microPOSIX BLE manager and ESP-IDF's BLE API.
 * 
 * Supports:
 * - Bluedroid (ESP32 classic Bluetooth stack)
 * - NimBLE (Apache Mynewt BLE stack ported to ESP-IDF)
 * - BLE Peripheral role
 * - GATT server functionality
 * - Connection management
 * - Message queue IPC for BLE events
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "microposix/ble/ble_mgr.h"
#include "microposix/kernel/mq.h"
#include "microposix/kernel/thread.h"
#include "microposix/debug/log.h"

// ESP-IDF includes
#ifdef CONFIG_BT_ENABLE
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#endif

// BLE backend configuration
#define MP_BLE_ESP32_MAX_CONNECTIONS 4
#define MP_BLE_ESP32_MTU 247
#define MP_BLE_ESP32_RX_QUEUE_SIZE 32
#define MP_BLE_ESP32_TX_QUEUE_SIZE 32

// BLE state
static mp_ble_state_t ble_state = MP_BLE_DISCONNECTED;

// Connection parameters
static mp_ble_conn_params_t conn_params = {
    .min_interval = 0x0006,  // 7.5ms
    .max_interval = 0x000C,  // 15ms
    .latency = 0,
    .supervision_timeout = 0x03E8  // 1000ms
};

// Message queues for BLE data
static mp_mq_t ble_rx_mq;
static mp_mq_t ble_tx_mq;
static uint8_t ble_rx_buffer[MP_BLE_ESP32_RX_QUEUE_SIZE * MP_BLE_ESP32_MTU];
static uint8_t ble_tx_buffer[MP_BLE_ESP32_TX_QUEUE_SIZE * MP_BLE_ESP32_MTU];

// BLE event queue (for ESP-IDF events)
static mp_mq_t ble_event_mq;
static uint8_t ble_event_buffer[32 * sizeof(mp_ble_event_t)];

// GATT service and characteristic handles
static uint16_t gatt_service_handle = 0;
static uint16_t gatt_char_handle = 0;

// Device information
static char device_name[32] = "microPOSIX-ESP32";
static uint8_t device_address[6] = {0};

// BLE timing oracle for tickless idle
static uint32_t next_ble_anchor_ticks = 0xFFFFFFFF;

/**
 * @brief Initialize BLE backend for ESP32
 */
int mp_ble_esp32_init(void) {
    MP_LOGI("BLE", "Initializing ESP32 BLE backend");
    
    #ifndef CONFIG_BT_ENABLE
    MP_LOGE("BLE", "Bluetooth not enabled in ESP-IDF configuration");
    MP_LOGE("BLE", "Please enable CONFIG_BT_ENABLE in menuconfig");
    return -1;
    #endif
    
    // Initialize message queues
    if (mp_mq_init(&ble_rx_mq, ble_rx_buffer, MP_BLE_ESP32_MTU, MP_BLE_ESP32_RX_QUEUE_SIZE) != 0) {
        MP_LOGE("BLE", "Failed to initialize RX message queue");
        return -1;
    }
    
    if (mp_mq_init(&ble_tx_mq, ble_tx_buffer, MP_BLE_ESP32_MTU, MP_BLE_ESP32_TX_QUEUE_SIZE) != 0) {
        MP_LOGE("BLE", "Failed to initialize TX message queue");
        return -1;
    }
    
    if (mp_mq_init(&ble_event_mq, ble_event_buffer, sizeof(mp_ble_event_t), 32) != 0) {
        MP_LOGE("BLE", "Failed to initialize event message queue");
        return -1;
    }
    
    // Initialize ESP-IDF Bluetooth stack
    esp_err_t err = esp_bt_controller_init();
    if (err != ESP_OK) {
        MP_LOGE("BLE", "Failed to initialize Bluetooth controller: %s", esp_err_to_name(err));
        return -1;
    }
    
    err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (err != ESP_OK) {
        MP_LOGE("BLE", "Failed to enable Bluetooth controller: %s", esp_err_to_name(err));
        return -1;
    }
    
    // Initialize Bluedroid stack
    err = esp_bluedroid_init();
    if (err != ESP_OK) {
        MP_LOGE("BLE", "Failed to initialize Bluedroid: %s", esp_err_to_name(err));
        return -1;
    }
    
    err = esp_bluedroid_enable();
    if (err != ESP_OK) {
        MP_LOGE("BLE", "Failed to enable Bluedroid: %s", esp_err_to_name(err));
        return -1;
    }
    
    // Register BLE callback
    err = esp_ble_gap_register_callback(mp_ble_esp32_gap_callback);
    if (err != ESP_OK) {
        MP_LOGE("BLE", "Failed to register GAP callback: %s", esp_err_to_name(err));
        return -1;
    }
    
    // Register GATT callback
    err = esp_ble_gatts_register_callback(mp_ble_esp32_gatts_callback);
    if (err != ESP_OK) {
        MP_LOGE("BLE", "Failed to register GATTS callback: %s", esp_err_to_name(err));
        return -1;
    }
    
    // Register GATTC callback
    err = esp_ble_gattc_register_callback(mp_ble_esp32_gattc_callback);
    if (err != ESP_OK) {
        MP_LOGE("BLE", "Failed to register GATTC callback: %s", esp_err_to_name(err));
        return -1;
    }
    
    // Get device address
    err = esp_bt_dev_get_address(device_address);
    if (err != ESP_OK) {
        MP_LOGW("BLE", "Failed to get device address: %s", esp_err_to_name(err));
    }
    
    // Set device name
    err = esp_ble_gap_set_device_name(device_name);
    if (err != ESP_OK) {
        MP_LOGW("BLE", "Failed to set device name: %s", esp_err_to_name(err));
    }
    
    // Initialize GATT server
    mp_ble_esp32_gatt_server_init();
    
    ble_state = MP_BLE_READY;
    MP_LOGI("BLE", "ESP32 BLE backend initialized successfully");
    MP_LOGI("BLE", "Device Name: %s", device_name);
    MP_LOGI("BLE", "Device Address: %02X:%02X:%02X:%02X:%02X:%02X",
            device_address[0], device_address[1], device_address[2],
            device_address[3], device_address[4], device_address[5]);
    
    return 0;
}

/**
 * @brief Deinitialize BLE backend
 */
void mp_ble_esp32_deinit(void) {
    MP_LOGI("BLE", "Deinitializing ESP32 BLE backend");
    
    ble_state = MP_BLE_DISCONNECTED;
    
    // Deinitialize ESP-IDF Bluetooth stack
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
    
    MP_LOGI("BLE", "ESP32 BLE backend deinitialized");
}

/**
 * @brief Start BLE advertising
 */
void mp_ble_esp32_start_adv(void) {
    if (ble_state != MP_BLE_READY && ble_state != MP_BLE_DISCONNECTED) {
        MP_LOGW("BLE", "Cannot start advertising, invalid state: %d", ble_state);
        return;
    }
    
    MP_LOGI("BLE", "Starting BLE advertising");
    
    // Set advertising parameters
    esp_ble_adv_params_t adv_params = {
        .adv_int_min = 0x20,  // 32 * 0.625ms = 20ms
        .adv_int_max = 0x40,  // 64 * 0.625ms = 40ms
        .adv_type = ADV_TYPE_IND,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .peer_addr = {0},
        .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .channel_map = ADV_CHNL_ALL,
        .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };
    
    // Set advertising data
    esp_ble_adv_data_t adv_data = {
        .set_scan_rsp = false,
        .include_name = true,
        .include_txpower = false,
        .min_interval = conn_params.min_interval,
        .max_interval = conn_params.max_interval,
        .appearance = 0x00,  // Generic
        .manufacturer_len = 0,
        .p_manufacturer_data = NULL,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = 0,
        .p_service_uuid = NULL,
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SUP),
    };
    
    // Set scan response data
    esp_ble_adv_data_t scan_rsp = {
        .set_scan_rsp = true,
        .include_name = true,
        .include_txpower = false,
        .min_interval = conn_params.min_interval,
        .max_interval = conn_params.max_interval,
        .appearance = 0x00,
        .manufacturer_len = 0,
        .p_manufacturer_data = NULL,
        .service_data_len = 0,
        .p_service_data = NULL,
        .service_uuid_len = 0,
        .p_service_uuid = NULL,
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SUP),
    };
    
    // Set advertising parameters
    esp_err_t err = esp_ble_gap_config_adv_data(&adv_data);
    if (err != ESP_OK) {
        MP_LOGE("BLE", "Failed to set advertising data: %s", esp_err_to_name(err));
        return;
    }
    
    err = esp_ble_gap_config_scan_rsp_data(&scan_rsp);
    if (err != ESP_OK) {
        MP_LOGE("BLE", "Failed to set scan response data: %s", esp_err_to_name(err));
        return;
    }
    
    err = esp_ble_gap_set_adv_params(&adv_params);
    if (err != ESP_OK) {
        MP_LOGE("BLE", "Failed to set advertising parameters: %s", esp_err_to_name(err));
        return;
    }
    
    // Start advertising
    err = esp_ble_gap_start_advertising();
    if (err != ESP_OK) {
        MP_LOGE("BLE", "Failed to start advertising: %s", esp_err_to_name(err));
        return;
    }
    
    ble_state = MP_BLE_ADVERTISING;
    MP_LOGI("BLE", "BLE advertising started");
}

/**
 * @brief Stop BLE advertising
 */
void mp_ble_esp32_stop_adv(void) {
    if (ble_state != MP_BLE_ADVERTISING) {
        MP_LOGW("BLE", "Cannot stop advertising, invalid state: %d", ble_state);
        return;
    }
    
    MP_LOGI("BLE", "Stopping BLE advertising");
    
    esp_err_t err = esp_ble_gap_stop_advertising();
    if (err != ESP_OK) {
        MP_LOGE("BLE", "Failed to stop advertising: %s", esp_err_to_name(err));
        return;
    }
    
    ble_state = MP_BLE_READY;
    MP_LOGI("BLE", "BLE advertising stopped");
}

/**
 * @brief Get current BLE state
 */
mp_ble_state_t mp_ble_esp32_get_state(void) {
    return ble_state;
}

/**
 * @brief Set connection parameters
 */
void mp_ble_esp32_set_conn_params(mp_ble_conn_params_t *params) {
    if (params == NULL) {
        return;
    }
    
    memcpy(&conn_params, params, sizeof(mp_ble_conn_params_t));
    MP_LOGI("BLE", "Connection parameters updated: min=%d, max=%d, latency=%d, timeout=%d",
            conn_params.min_interval, conn_params.max_interval,
            conn_params.latency, conn_params.supervision_timeout);
}

/**
 * @brief Get connection parameters
 */
void mp_ble_esp32_get_conn_params(mp_ble_conn_params_t *params) {
    if (params == NULL) {
        return;
    }
    
    memcpy(params, &conn_params, sizeof(mp_ble_conn_params_t));
}

/**
 * @brief Send data over BLE
 */
int mp_ble_esp32_send_data(uint16_t conn_handle, uint8_t *data, uint16_t len) {
    if (ble_state != MP_BLE_CONNECTED) {
        MP_LOGE("BLE", "Cannot send data, not connected");
        return -1;
    }
    
    if (data == NULL || len == 0 || len > MP_BLE_ESP32_MTU) {
        MP_LOGE("BLE", "Invalid data or length: %d", len);
        return -1;
    }
    
    // Check if we can send directly or need to queue
    esp_err_t err = esp_ble_gatts_send_indicate(gatt_service_handle, conn_handle, gatt_char_handle,
                                                len, data, false);
    if (err != ESP_OK) {
        // Queue the data for later transmission
        if (mp_mq_send(&ble_tx_mq, data, 100) != 0) {
            MP_LOGE("BLE", "Failed to queue data for transmission");
            return -1;
        }
        return 0;
    }
    
    return len;
}

/**
 * @brief Receive data from BLE (non-blocking)
 */
int mp_ble_esp32_receive_data(uint8_t *data, uint16_t max_len, uint32_t timeout_ms) {
    if (data == NULL || max_len == 0) {
        return -1;
    }
    
    return mp_mq_receive(&ble_rx_mq, data, timeout_ms);
}

/**
 * @brief Get BLE timing oracle for tickless idle
 * 
 * Returns the number of ticks until the next BLE connection event.
 * This allows the scheduler to wake up before the connection event.
 */
uint32_t mp_ble_esp32_get_next_anchor_ticks(void) {
    // If we have an active connection, calculate based on connection interval
    if (ble_state == MP_BLE_CONNECTED) {
        // Connection interval in ticks (assuming 1ms tick)
        // For 7.5ms interval: 7.5 * 1.25 = 9.375ms wakeup before
        // Convert connection interval (in 1.25ms units) to ticks
        uint32_t conn_interval_ticks = conn_params.min_interval * 0.8;  // 80% of interval
        return conn_interval_ticks;
    }
    
    // If advertising, wake up before next advertising packet
    if (ble_state == MP_BLE_ADVERTISING) {
        // Advertising interval is 20-40ms, wake up at 15ms
        return 15;
    }
    
    // No BLE activity, can sleep indefinitely
    return 0xFFFFFFFF;
}

/**
 * @brief GAP callback handler
 */
static void mp_ble_esp32_gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
    mp_ble_event_t ble_event;
    
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            MP_LOGD("BLE", "Advertising data set complete");
            break;
            
        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            MP_LOGD("BLE", "Scan response data set complete");
            break;
            
        case ESP_GAP_BLE_SCAN_RESULT_EVT:
            // Device discovered during scanning
            MP_LOGD("BLE", "Device discovered: %02X:%02X:%02X:%02X:%02X:%02X",
                    param->scan_rst.bda[0], param->scan_rst.bda[1], param->scan_rst.bda[2],
                    param->scan_rst.bda[3], param->scan_rst.bda[4], param->scan_rst.bda[5]);
            break;
            
        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
            MP_LOGD("BLE", "Scan stopped");
            break;
            
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            MP_LOGI("BLE", "Advertising started");
            ble_state = MP_BLE_ADVERTISING;
            
            ble_event.type = MP_BLE_EVENT_ADV_STARTED;
            mp_mq_send(&ble_event_mq, &ble_event, 0);
            break;
            
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            MP_LOGI("BLE", "Advertising stopped");
            ble_state = MP_BLE_READY;
            
            ble_event.type = MP_BLE_EVENT_ADV_STOPPED;
            mp_mq_send(&ble_event_mq, &ble_event, 0);
            break;
            
        case ESP_GAP_BLE_CONNECT_EVT:
            MP_LOGI("BLE", "Device connected: %02X:%02X:%02X:%02X:%02X:%02X",
                    param->connect.bda[0], param->connect.bda[1], param->connect.bda[2],
                    param->connect.bda[3], param->connect.bda[4], param->connect.bda[5]);
            ble_state = MP_BLE_CONNECTED;
            
            ble_event.type = MP_BLE_EVENT_CONNECTED;
            ble_event.conn_handle = param->connect.conn_handle;
            memcpy(ble_event.address, param->connect.bda, 6);
            mp_mq_send(&ble_event_mq, &ble_event, 0);
            break;
            
        case ESP_GAP_BLE_DISCONNECT_EVT:
            MP_LOGI("BLE", "Device disconnected");
            ble_state = MP_BLE_DISCONNECTED;
            
            ble_event.type = MP_BLE_EVENT_DISCONNECTED;
            ble_event.conn_handle = param->disconnect.conn_handle;
            mp_mq_send(&ble_event_mq, &ble_event, 0);
            
            // Restart advertising
            mp_ble_esp32_start_adv();
            break;
            
        case ESP_GAP_BLE_CONN_PARAM_UPDATE_EVT:
            MP_LOGI("BLE", "Connection parameters updated");
            ble_event.type = MP_BLE_EVENT_CONN_PARAM_UPDATE;
            ble_event.conn_handle = param->conn_param_update.conn_handle;
            mp_mq_send(&ble_event_mq, &ble_event, 0);
            break;
            
        case ESP_GAP_BLE_SET_STATIC_RAND_ADDR_EVT:
            MP_LOGD("BLE", "Static random address set");
            break;
            
        case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
            MP_LOGD("BLE", "Connection parameters update request");
            break;
            
        default:
            MP_LOGD("BLE", "Unhandled GAP event: %d", event);
            break;
    }
}

/**
 * @brief GATT Server callback handler
 */
static void mp_ble_esp32_gatts_callback(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if,
                                       esp_ble_gatts_cb_param_t *param) {
    mp_ble_event_t ble_event;
    
    switch (event) {
        case ESP_GATTS_REG_EVT:
            MP_LOGD("BLE", "GATT server registered, app_id: %d", param->reg.app_id);
            break;
            
        case ESP_GATTS_READ_EVT:
            MP_LOGD("BLE", "GATT read request");
            break;
            
        case ESP_GATTS_WRITE_EVT:
            MP_LOGD("BLE", "GATT write request, handle: %d, len: %d", 
                    param->write.handle, param->write.len);
            
            // Forward received data to RX queue
            if (param->write.len > 0 && param->write.value != NULL) {
                uint8_t *data = malloc(param->write.len);
                if (data != NULL) {
                    memcpy(data, param->write.value, param->write.len);
                    mp_mq_send(&ble_rx_mq, data, 0);
                    free(data);
                }
            }
            
            ble_event.type = MP_BLE_EVENT_DATA_RECEIVED;
            ble_event.conn_handle = param->write.conn_id;
            ble_event.data_length = param->write.len;
            mp_mq_send(&ble_event_mq, &ble_event, 0);
            break;
            
        case ESP_GATTS_EXEC_WRITE_EVT:
            MP_LOGD("BLE", "GATT execute write");
            break;
            
        case ESP_GATTS_MTU_EVT:
            MP_LOGI("BLE", "MTU exchange: %d", param->mtu.mtu);
            break;
            
        case ESP_GATTS_CONF_EVT:
            MP_LOGD("BLE", "GATT confirmation");
            break;
            
        case ESP_GATTS_START_EVT:
            MP_LOGD("BLE", "GATT server started");
            break;
            
        case ESP_GATTS_STOP_EVT:
            MP_LOGD("BLE", "GATT server stopped");
            break;
            
        case ESP_GATTS_CONNECT_EVT:
            MP_LOGD("BLE", "GATT server connected");
            break;
            
        case ESP_GATTS_DISCONNECT_EVT:
            MP_LOGD("BLE", "GATT server disconnected");
            break;
            
        case ESP_GATTS_CREAT_ATTR_TAB_EVT:
            MP_LOGD("BLE", "GATT attribute table created");
            break;
            
        case ESP_GATTS_SET_ATTR_VAL_EVT:
            MP_LOGD("BLE", "GATT attribute value set");
            break;
            
        default:
            MP_LOGD("BLE", "Unhandled GATTS event: %d", event);
            break;
    }
}

/**
 * @brief GATT Client callback handler
 */
static void mp_ble_esp32_gattc_callback(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if,
                                       esp_ble_gattc_cb_param_t *param) {
    mp_ble_event_t ble_event;
    
    switch (event) {
        case ESP_GATTC_REG_EVT:
            MP_LOGD("BLE", "GATT client registered, app_id: %d", param->reg.app_id);
            break;
            
        case ESP_GATTC_CONNECT_EVT:
            MP_LOGD("BLE", "GATT client connected");
            break;
            
        case ESP_GATTC_DISCONNECT_EVT:
            MP_LOGD("BLE", "GATT client disconnected");
            break;
            
        case ESP_GATTC_SEARCH_CMPL_EVT:
            MP_LOGD("BLE", "GATT client search complete");
            break;
            
        case ESP_GATTC_SEARCH_RES_EVT:
            MP_LOGD("BLE", "GATT client search result");
            break;
            
        case ESP_GATTC_READ_CHAR_EVT:
            MP_LOGD("BLE", "GATT client read characteristic");
            break;
            
        case ESP_GATTC_WRITE_CHAR_EVT:
            MP_LOGD("BLE", "GATT client write characteristic");
            break;
            
        case ESP_GATTC_NOTIFY_EVT:
            MP_LOGD("BLE", "GATT client notification received");
            break;
            
        case ESP_GATTC_INDICATE_EVT:
            MP_LOGD("BLE", "GATT client indication received");
            break;
            
        default:
            MP_LOGD("BLE", "Unhandled GATTC event: %d", event);
            break;
    }
}

/**
 * @brief Initialize GATT server
 */
static void mp_ble_esp32_gatt_server_init(void) {
    // Create GATT service
    esp_gatts_attr_db_t gatt_db[20];
    
    // Service declaration
    gatt_db[0] = {
        .attr_control = {.uuid_len = ESP_UUID_LEN_16, .uuid_p = (uint8_t *)ESP_GATT_UUID_PRI_SERVICE,
                         .perm = ESP_GATT_PERM_READ, .max_length = 2, .length = 2},
        .attr_value = {.attr_len = 2, .attr_max_len = 2, .attr_val = (uint8_t *)\{0x00, 0x18\}},
    };
    
    // Characteristic declaration
    gatt_db[1] = {
        .attr_control = {.uuid_len = ESP_UUID_LEN_16, .uuid_p = (uint8_t *)ESP_GATT_UUID_CHAR_DECLARE,
                         .perm = ESP_GATT_PERM_READ, .max_length = 1, .length = 1},
        .attr_value = {.attr_len = 1, .attr_max_len = 1, .attr_val = (uint8_t *)\{0x02\}},
    };
    
    // Characteristic value
    gatt_db[2] = {
        .attr_control = {.uuid_len = ESP_UUID_LEN_16, .uuid_p = (uint8_t *)ESP_GATT_UUID_MP_DATA,
                         .perm = ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE | ESP_GATT_PERM_NOTIFY,
                         .max_length = MP_BLE_ESP32_MTU, .length = 0},
        .attr_value = {.attr_len = 0, .attr_max_len = MP_BLE_ESP32_MTU, .attr_val = NULL},
    };
    
    // Register GATT service
    esp_err_t err = esp_ble_gatts_app_register(0, gatt_db, 3);
    if (err != ESP_OK) {
        MP_LOGE("BLE", "Failed to register GATT service: %s", esp_err_to_name(err));
    }
}

/**
 * @brief BLE host task for ESP32
 * 
 * This task handles BLE events and data processing.
 */
void *mp_ble_esp32_host_task(void *arg) {
    (void)arg;
    
    MP_LOGI("BLE", "BLE host task started");
    
    // Initialize BLE
    if (mp_ble_esp32_init() != 0) {
        MP_LOGE("BLE", "Failed to initialize BLE");
        return NULL;
    }
    
    // Start advertising
    mp_ble_esp32_start_adv();
    
    while (1) {
        // Process BLE events
        mp_ble_event_t event;
        if (mp_mq_receive(&ble_event_mq, &event, 100) == 0) {
            mp_ble_esp32_process_event(&event);
        }
        
        // Process TX queue
        uint8_t tx_data[MP_BLE_ESP32_MTU];
        if (mp_mq_receive(&ble_tx_mq, tx_data, 0) == 0) {
            // Try to send queued data
            // In a real implementation, we'd track connection handles
            // and send to the appropriate connection
        }
        
        // Yield to other tasks
        mp_thread_sleep(10);
    }
    
    return NULL;
}

/**
 * @brief Process BLE events
 */
static void mp_ble_esp32_process_event(mp_ble_event_t *event) {
    if (event == NULL) {
        return;
    }
    
    switch (event->type) {
        case MP_BLE_EVENT_ADV_STARTED:
            MP_LOGI("BLE", "Advertising started event");
            break;
            
        case MP_BLE_EVENT_ADV_STOPPED:
            MP_LOGI("BLE", "Advertising stopped event");
            break;
            
        case MP_BLE_EVENT_CONNECTED:
            MP_LOGI("BLE", "Connected event, handle: %d", event->conn_handle);
            break;
            
        case MP_BLE_EVENT_DISCONNECTED:
            MP_LOGI("BLE", "Disconnected event, handle: %d", event->conn_handle);
            break;
            
        case MP_BLE_EVENT_DATA_RECEIVED:
            MP_LOGI("BLE", "Data received event, length: %d", event->data_length);
            break;
            
        case MP_BLE_EVENT_CONN_PARAM_UPDATE:
            MP_LOGI("BLE", "Connection parameters updated event");
            break;
            
        default:
            MP_LOGW("BLE", "Unknown event type: %d", event->type);
            break;
    }
}

/**
 * @brief Get BLE RX message queue
 */
mp_mq_t *mp_ble_esp32_get_rx_mq(void) {
    return &ble_rx_mq;
}

/**
 * @brief Get BLE TX message queue
 */
mp_mq_t *mp_ble_esp32_get_tx_mq(void) {
    return &ble_tx_mq;
}

/**
 * @brief Get BLE event message queue
 */
mp_mq_t *mp_ble_esp32_get_event_mq(void) {
    return &ble_event_mq;
}
