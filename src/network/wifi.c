/**
 * microPOSIX WiFi Driver for ESP32
 * 
 * This file implements a platform-agnostic WiFi driver with ESP32-specific backend.
 * It provides a consistent API for WiFi connectivity across different platforms.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "microposix/network/wifi.h"
#include "microposix/debug/log.h"
#include "microposix/kernel/thread.h"

// Platform detection
#if defined(MICROPOSIX_PLATFORM_ESP32)
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#define WIFI_BACKEND_ESP32 1
#endif

// WiFi state
static mp_wifi_state_t wifi_state = MP_WIFI_DISCONNECTED;

// WiFi configuration
static mp_wifi_config_t wifi_config = {
    .ssid = "",
    .password = "",
    .bssid = {0},
    .channel = 0,
    .authmode = MP_WIFI_AUTH_WPA2_PSK
};

// WiFi event callback
static mp_wifi_event_callback_t wifi_event_callback = NULL;

// WiFi connection status callback
static mp_wifi_conn_status_callback_t wifi_conn_status_callback = NULL;

// WiFi scan results callback
static mp_wifi_scan_callback_t wifi_scan_callback = NULL;

// WiFi event handler task
static mp_thread_id_t wifi_task_id = 0;

// Mutex for thread-safe access
static bool wifi_mutex_locked = false;

/**
 * @brief Lock WiFi mutex
 */
static void mp_wifi_lock(void) {
    // In a real implementation, use a proper mutex
    // For now, we'll use a simple spinlock
    while (wifi_mutex_locked) {
        mp_thread_sleep(1);
    }
    wifi_mutex_locked = true;
}

/**
 * @brief Unlock WiFi mutex
 */
static void mp_wifi_unlock(void) {
    wifi_mutex_locked = false;
}

/**
 * @brief Initialize WiFi driver
 */
int mp_wifi_init(void) {
    MP_LOGI("WIFI", "Initializing WiFi driver");
    
    #if defined(WIFI_BACKEND_ESP32)
    // Initialize NVS (required for WiFi)
    esp_err_t err = nvs_flash_init();
    if (err != ESP_OK) {
        MP_LOGE("WIFI", "Failed to initialize NVS: %s", esp_err_to_name(err));
        return -1;
    }
    
    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        MP_LOGE("WIFI", "Failed to initialize WiFi: %s", esp_err_to_name(err));
        return -1;
    }
    
    // Register WiFi event handler
    err = esp_event_loop_create_default();
    if (err != ESP_OK) {
        MP_LOGE("WIFI", "Failed to create event loop: %s", esp_err_to_name(err));
        return -1;
    }
    
    // Register event handlers
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    
    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &mp_wifi_esp32_event_handler, NULL, &instance_any_id);
    if (err != ESP_OK) {
        MP_LOGE("WIFI", "Failed to register WiFi event handler: %s", esp_err_to_name(err));
        return -1;
    }
    
    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &mp_wifi_esp32_event_handler, NULL, &instance_got_ip);
    if (err != ESP_OK) {
        MP_LOGE("WIFI", "Failed to register IP event handler: %s", esp_err_to_name(err));
        return -1;
    }
    
    // Start WiFi task
    mp_wifi_task_id = mp_thread_create(mp_wifi_task, NULL, NULL);
    if (mp_wifi_task_id == 0) {
        MP_LOGE("WIFI", "Failed to create WiFi task");
        return -1;
    }
    
    #endif
    
    wifi_state = MP_WIFI_READY;
    MP_LOGI("WIFI", "WiFi driver initialized");
    return 0;
}

/**
 * @brief Deinitialize WiFi driver
 */
void mp_wifi_deinit(void) {
    MP_LOGI("WIFI", "Deinitializing WiFi driver");
    
    mp_wifi_disconnect();
    
    #if defined(WIFI_BACKEND_ESP32)
    esp_wifi_stop();
    esp_wifi_deinit();
    #endif
    
    wifi_state = MP_WIFI_DISCONNECTED;
    MP_LOGI("WIFI", "WiFi driver deinitialized");
}

/**
 * @brief Set WiFi configuration
 */
void mp_wifi_set_config(mp_wifi_config_t *config) {
    if (config == NULL) {
        return;
    }
    
    mp_wifi_lock();
    memcpy(&wifi_config, config, sizeof(mp_wifi_config_t));
    mp_wifi_unlock();
    
    MP_LOGI("WIFI", "WiFi configuration updated: SSID=%s", wifi_config.ssid);
}

/**
 * @brief Get WiFi configuration
 */
void mp_wifi_get_config(mp_wifi_config_t *config) {
    if (config == NULL) {
        return;
    }
    
    mp_wifi_lock();
    memcpy(config, &wifi_config, sizeof(mp_wifi_config_t));
    mp_wifi_unlock();
}

/**
 * @brief Connect to WiFi network
 */
int mp_wifi_connect(void) {
    if (wifi_state != MP_WIFI_READY && wifi_state != MP_WIFI_DISCONNECTED) {
        MP_LOGW("WIFI", "Cannot connect, invalid state: %d", wifi_state);
        return -1;
    }
    
    if (wifi_config.ssid[0] == '\0') {
        MP_LOGE("WIFI", "SSID not configured");
        return -1;
    }
    
    MP_LOGI("WIFI", "Connecting to WiFi network: %s", wifi_config.ssid);
    
    #if defined(WIFI_BACKEND_ESP32)
    // Set WiFi mode to station
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        MP_LOGE("WIFI", "Failed to set WiFi mode: %s", esp_err_to_name(err));
        return -1;
    }
    
    // Configure WiFi
    wifi_config_t sta_config = {
        .sta = {
            .ssid = {0},
            .password = {0},
            .threshold.authmode = mp_wifi_auth_to_esp(wifi_config.authmode),
        },
    };
    
    strncpy((char *)sta_config.sta.ssid, wifi_config.ssid, sizeof(sta_config.sta.ssid));
    strncpy((char *)sta_config.sta.password, wifi_config.password, sizeof(sta_config.sta.password));
    
    err = esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config);
    if (err != ESP_OK) {
        MP_LOGE("WIFI", "Failed to set WiFi config: %s", esp_err_to_name(err));
        return -1;
    }
    
    // Start WiFi
    err = esp_wifi_start();
    if (err != ESP_OK) {
        MP_LOGE("WIFI", "Failed to start WiFi: %s", esp_err_to_name(err));
        return -1;
    }
    
    // Connect
    err = esp_wifi_connect();
    if (err != ESP_OK) {
        MP_LOGE("WIFI", "Failed to connect to WiFi: %s", esp_err_to_name(err));
        return -1;
    }
    
    #endif
    
    wifi_state = MP_WIFI_CONNECTING;
    MP_LOGI("WIFI", "WiFi connection initiated");
    return 0;
}

/**
 * @brief Disconnect from WiFi network
 */
void mp_wifi_disconnect(void) {
    if (wifi_state != MP_WIFI_CONNECTED && wifi_state != MP_WIFI_CONNECTING) {
        MP_LOGW("WIFI", "Cannot disconnect, invalid state: %d", wifi_state);
        return;
    }
    
    MP_LOGI("WIFI", "Disconnecting from WiFi network");
    
    #if defined(WIFI_BACKEND_ESP32)
    esp_wifi_disconnect();
    esp_wifi_stop();
    #endif
    
    wifi_state = MP_WIFI_DISCONNECTED;
    MP_LOGI("WIFI", "WiFi disconnected");
}

/**
 * @brief Get current WiFi state
 */
mp_wifi_state_t mp_wifi_get_state(void) {
    return wifi_state;
}

/**
 * @brief Get WiFi connection status
 */
void mp_wifi_get_status(mp_wifi_status_t *status) {
    if (status == NULL) {
        return;
    }
    
    #if defined(WIFI_BACKEND_ESP32)
    wifi_ap_record_t ap_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
    if (err == ESP_OK) {
        status->connected = true;
        status->rssi = ap_info.rssi;
        status->channel = ap_info.primary;
        memcpy(status->bssid, ap_info.bssid, 6);
    } else {
        status->connected = false;
        status->rssi = 0;
        status->channel = 0;
        memset(status->bssid, 0, 6);
    }
    
    // Get IP address
    tcpip_adapter_ip_info_t ip_info;
    err = tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ip_info);
    if (err == ESP_OK) {
        status->ip_addr = ip_info.ip.addr;
        status->netmask = ip_info.netmask.addr;
        status->gateway = ip_info.gw.addr;
    } else {
        status->ip_addr = 0;
        status->netmask = 0;
        status->gateway = 0;
    }
    
    #else
    // Simulation
    status->connected = (wifi_state == MP_WIFI_CONNECTED);
    status->rssi = -50;
    status->channel = 6;
    status->ip_addr = 0xC0A80101;  // 192.168.1.1
    status->netmask = 0xFFFFFF00;   // 255.255.255.0
    status->gateway = 0xC0A80101;   // 192.168.1.1
    memset(status->bssid, 0, 6);
    #endif
}

/**
 * @brief Start WiFi access point (AP) mode
 */
int mp_wifi_start_ap(mp_wifi_ap_config_t *config) {
    if (config == NULL) {
        return -1;
    }
    
    MP_LOGI("WIFI", "Starting WiFi AP: %s", config->ssid);
    
    #if defined(WIFI_BACKEND_ESP32)
    // Set WiFi mode to AP
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) {
        MP_LOGE("WIFI", "Failed to set WiFi mode: %s", esp_err_to_name(err));
        return -1;
    }
    
    // Configure AP
    wifi_config_t ap_config = {
        .ap = {
            .ssid = {0},
            .ssid_len = 0,
            .channel = config->channel,
            .password = {0},
            .max_connection = config->max_connections,
            .authmode = mp_wifi_auth_to_esp(config->authmode),
        },
    };
    
    strncpy((char *)ap_config.ap.ssid, config->ssid, sizeof(ap_config.ap.ssid));
    ap_config.ap.ssid_len = strlen(config->ssid);
    
    if (config->password[0] != '\0') {
        strncpy((char *)ap_config.ap.password, config->password, sizeof(ap_config.ap.password));
    }
    
    err = esp_wifi_set_config(ESP_IF_WIFI_AP, &ap_config);
    if (err != ESP_OK) {
        MP_LOGE("WIFI", "Failed to set AP config: %s", esp_err_to_name(err));
        return -1;
    }
    
    // Start WiFi
    err = esp_wifi_start();
    if (err != ESP_OK) {
        MP_LOGE("WIFI", "Failed to start WiFi: %s", esp_err_to_name(err));
        return -1;
    }
    
    #endif
    
    wifi_state = MP_WIFI_AP_MODE;
    MP_LOGI("WIFI", "WiFi AP started");
    return 0;
}

/**
 * @brief Stop WiFi access point (AP) mode
 */
void mp_wifi_stop_ap(void) {
    if (wifi_state != MP_WIFI_AP_MODE) {
        MP_LOGW("WIFI", "Cannot stop AP, invalid state: %d", wifi_state);
        return;
    }
    
    MP_LOGI("WIFI", "Stopping WiFi AP");
    
    #if defined(WIFI_BACKEND_ESP32)
    esp_wifi_stop();
    #endif
    
    wifi_state = MP_WIFI_READY;
    MP_LOGI("WIFI", "WiFi AP stopped");
}

/**
 * @brief Start WiFi scan
 */
int mp_wifi_scan_start(void) {
    if (wifi_state != MP_WIFI_READY) {
        MP_LOGW("WIFI", "Cannot start scan, invalid state: %d", wifi_state);
        return -1;
    }
    
    MP_LOGI("WIFI", "Starting WiFi scan");
    
    #if defined(WIFI_BACKEND_ESP32)
    // Set WiFi mode to station for scanning
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        MP_LOGE("WIFI", "Failed to set WiFi mode: %s", esp_err_to_name(err));
        return -1;
    }
    
    // Start WiFi for scanning
    err = esp_wifi_start();
    if (err != ESP_OK) {
        MP_LOGE("WIFI", "Failed to start WiFi: %s", esp_err_to_name(err));
        return -1;
    }
    
    // Start scan
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true
    };
    
    err = esp_wifi_scan_start(&scan_config, true);
    if (err != ESP_OK) {
        MP_LOGE("WIFI", "Failed to start scan: %s", esp_err_to_name(err));
        return -1;
    }
    
    #endif
    
    wifi_state = MP_WIFI_SCANNING;
    return 0;
}

/**
 * @brief Get WiFi scan results
 */
int mp_wifi_scan_get_results(mp_wifi_ap_info_t *aps, int max_aps) {
    if (aps == NULL || max_aps <= 0) {
        return -1;
    }
    
    #if defined(WIFI_BACKEND_ESP32)
    uint16_t ap_count = 0;
    esp_err_t err = esp_wifi_scan_get_ap_num(&ap_count);
    if (err != ESP_OK) {
        MP_LOGE("WIFI", "Failed to get AP count: %s", esp_err_to_name(err));
        return -1;
    }
    
    if (ap_count == 0) {
        return 0;
    }
    
    // Allocate buffer for scan results
    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (ap_records == NULL) {
        MP_LOGE("WIFI", "Failed to allocate memory for AP records");
        return -1;
    }
    
    err = esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    if (err != ESP_OK) {
        MP_LOGE("WIFI", "Failed to get AP records: %s", esp_err_to_name(err));
        free(ap_records);
        return -1;
    }
    
    // Copy results to output buffer
    int count = (ap_count < max_aps) ? ap_count : max_aps;
    for (int i = 0; i < count; i++) {
        strncpy(aps[i].ssid, (char *)ap_records[i].ssid, sizeof(aps[i].ssid));
        aps[i].rssi = ap_records[i].rssi;
        aps[i].channel = ap_records[i].primary;
        aps[i].authmode = mp_wifi_auth_from_esp(ap_records[i].authmode);
        memcpy(aps[i].bssid, ap_records[i].bssid, 6);
    }
    
    free(ap_records);
    return count;
    
    #else
    // Simulation - return dummy data
    if (max_aps > 0) {
        strncpy(aps[0].ssid, "TestNetwork", sizeof(aps[0].ssid));
        aps[0].rssi = -50;
        aps[0].channel = 6;
        aps[0].authmode = MP_WIFI_AUTH_WPA2_PSK;
        memset(aps[0].bssid, 0, 6);
        return 1;
    }
    return 0;
    #endif
}

/**
 * @brief Set WiFi event callback
 */
void mp_wifi_set_event_callback(mp_wifi_event_callback_t callback) {
    wifi_event_callback = callback;
}

/**
 * @brief Set WiFi connection status callback
 */
void mp_wifi_set_conn_status_callback(mp_wifi_conn_status_callback_t callback) {
    wifi_conn_status_callback = callback;
}

/**
 * @brief Set WiFi scan callback
 */
void mp_wifi_set_scan_callback(mp_wifi_scan_callback_t callback) {
    wifi_scan_callback = callback;
}

/**
 * @brief WiFi task
 */
void *mp_wifi_task(void *arg) {
    (void)arg;
    
    MP_LOGI("WIFI", "WiFi task started");
    
    while (1) {
        // Check WiFi state and handle events
        switch (wifi_state) {
            case MP_WIFI_CONNECTING:
                // Wait for connection
                mp_thread_sleep(100);
                break;
                
            case MP_WIFI_CONNECTED:
                // Connection established
                if (wifi_conn_status_callback) {
                    mp_wifi_status_t status;
                    mp_wifi_get_status(&status);
                    wifi_conn_status_callback(true, &status);
                }
                mp_thread_sleep(1000);
                break;
                
            case MP_WIFI_DISCONNECTED:
                if (wifi_conn_status_callback) {
                    wifi_conn_status_callback(false, NULL);
                }
                mp_thread_sleep(100);
                break;
                
            case MP_WIFI_SCANNING:
                // Wait for scan to complete
                mp_thread_sleep(100);
                break;
                
            default:
                mp_thread_sleep(100);
                break;
        }
    }
    
    return NULL;
}

// ============================================================================
// ESP32-Specific Implementation
// ============================================================================

#if defined(WIFI_BACKEND_ESP32)

static const char *TAG = "mp_wifi_esp32";

/**
 * @brief Convert microPOSIX auth mode to ESP-IDF auth mode
 */
static wifi_auth_mode_t mp_wifi_auth_to_esp(mp_wifi_authmode_t authmode) {
    switch (authmode) {
        case MP_WIFI_AUTH_OPEN: return WIFI_AUTH_OPEN;
        case MP_WIFI_AUTH_WEP: return WIFI_AUTH_WEP;
        case MP_WIFI_AUTH_WPA_PSK: return WIFI_AUTH_WPA_PSK;
        case MP_WIFI_AUTH_WPA2_PSK: return WIFI_AUTH_WPA2_PSK;
        case MP_WIFI_AUTH_WPA_WPA2_PSK: return WIFI_AUTH_WPA_WPA2_PSK;
        case MP_WIFI_AUTH_WPA2_ENTERPRISE: return WIFI_AUTH_WPA2_ENTERPRISE;
        case MP_WIFI_AUTH_WPA3_PSK: return WIFI_AUTH_WPA3_PSK;
        case MP_WIFI_AUTH_WPA2_WPA3_PSK: return WIFI_AUTH_WPA2_WPA3_PSK;
        default: return WIFI_AUTH_OPEN;
    }
}

/**
 * @brief Convert ESP-IDF auth mode to microPOSIX auth mode
 */
static mp_wifi_authmode_t mp_wifi_auth_from_esp(wifi_auth_mode_t authmode) {
    switch (authmode) {
        case WIFI_AUTH_OPEN: return MP_WIFI_AUTH_OPEN;
        case WIFI_AUTH_WEP: return MP_WIFI_AUTH_WEP;
        case WIFI_AUTH_WPA_PSK: return MP_WIFI_AUTH_WPA_PSK;
        case WIFI_AUTH_WPA2_PSK: return MP_WIFI_AUTH_WPA2_PSK;
        case WIFI_AUTH_WPA_WPA2_PSK: return MP_WIFI_AUTH_WPA_WPA2_PSK;
        case WIFI_AUTH_WPA2_ENTERPRISE: return MP_WIFI_AUTH_WPA2_ENTERPRISE;
        case WIFI_AUTH_WPA3_PSK: return MP_WIFI_AUTH_WPA3_PSK;
        case WIFI_AUTH_WPA2_WPA3_PSK: return MP_WIFI_AUTH_WPA2_WPA3_PSK;
        default: return MP_WIFI_AUTH_OPEN;
    }
}

/**
 * @brief WiFi event handler for ESP32
 */
static void mp_wifi_esp32_event_handler(void *arg, esp_event_base_t event_base,
                                        int32_t event_id, void *event_data) {
    (void)arg;
    
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_WIFI_READY:
                MP_LOGI(TAG, "WiFi ready");
                break;
                
            case WIFI_EVENT_SCAN_DONE:
                MP_LOGI(TAG, "WiFi scan done");
                wifi_state = MP_WIFI_READY;
                if (wifi_scan_callback) {
                    mp_wifi_scan_callback();
                }
                break;
                
            case WIFI_EVENT_STA_START:
                MP_LOGI(TAG, "WiFi station started");
                break;
                
            case WIFI_EVENT_STA_STOP:
                MP_LOGI(TAG, "WiFi station stopped");
                wifi_state = MP_WIFI_READY;
                break;
                
            case WIFI_EVENT_STA_CONNECTED:
                MP_LOGI(TAG, "WiFi connected");
                wifi_state = MP_WIFI_CONNECTED;
                if (wifi_event_callback) {
                    wifi_event_callback(MP_WIFI_EVENT_CONNECTED);
                }
                break;
                
            case WIFI_EVENT_STA_DISCONNECTED:
                MP_LOGI(TAG, "WiFi disconnected");
                wifi_state = MP_WIFI_DISCONNECTED;
                if (wifi_event_callback) {
                    wifi_event_callback(MP_WIFI_EVENT_DISCONNECTED);
                }
                break;
                
            case WIFI_EVENT_STA_AUTHMODE_CHANGE:
                MP_LOGI(TAG, "WiFi auth mode changed");
                break;
                
            case WIFI_EVENT_AP_START:
                MP_LOGI(TAG, "WiFi AP started");
                wifi_state = MP_WIFI_AP_MODE;
                if (wifi_event_callback) {
                    wifi_event_callback(MP_WIFI_EVENT_AP_STARTED);
                }
                break;
                
            case WIFI_EVENT_AP_STOP:
                MP_LOGI(TAG, "WiFi AP stopped");
                wifi_state = MP_WIFI_READY;
                if (wifi_event_callback) {
                    wifi_event_callback(MP_WIFI_EVENT_AP_STOPPED);
                }
                break;
                
            case WIFI_EVENT_AP_STACONNECTED:
                MP_LOGI(TAG, "WiFi AP: Station connected");
                if (wifi_event_callback) {
                    wifi_event_callback(MP_WIFI_EVENT_AP_STA_CONNECTED);
                }
                break;
                
            case WIFI_EVENT_AP_STADISCONNECTED:
                MP_LOGI(TAG, "WiFi AP: Station disconnected");
                if (wifi_event_callback) {
                    wifi_event_callback(MP_WIFI_EVENT_AP_STA_DISCONNECTED);
                }
                break;
                
            default:
                MP_LOGD(TAG, "Unhandled WiFi event: %d", event_id);
                break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
            case IP_EVENT_STA_GOT_IP:
                MP_LOGI(TAG, "Got IP address");
                ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
                MP_LOGI(TAG, "IP: " IPSTR, IP2STR(&event->ip_info.ip));
                if (wifi_event_callback) {
                    wifi_event_callback(MP_WIFI_EVENT_GOT_IP);
                }
                break;
                
            case IP_EVENT_STA_LOST_IP:
                MP_LOGI(TAG, "Lost IP address");
                if (wifi_event_callback) {
                    wifi_event_callback(MP_WIFI_EVENT_LOST_IP);
                }
                break;
                
            default:
                MP_LOGD(TAG, "Unhandled IP event: %d", event_id);
                break;
        }
    }
}

#endif // WIFI_BACKEND_ESP32
