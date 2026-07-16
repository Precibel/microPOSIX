#ifndef MICROPOSIX_NETWORK_WIFI_H
#define MICROPOSIX_NETWORK_WIFI_H

#include <stdint.h>
#include <stdbool.h>

// WiFi states
typedef enum {
    MP_WIFI_DISCONNECTED,    // WiFi is not connected
    MP_WIFI_READY,           // WiFi is initialized and ready
    MP_WIFI_CONNECTING,      // WiFi is connecting
    MP_WIFI_CONNECTED,       // WiFi is connected
    MP_WIFI_AP_MODE,         // WiFi is in AP mode
    MP_WIFI_SCANNING,        // WiFi is scanning
    MP_WIFI_ERROR            // WiFi error state
} mp_wifi_state_t;

// WiFi authentication modes
typedef enum {
    MP_WIFI_AUTH_OPEN,              // Open network
    MP_WIFI_AUTH_WEP,               // WEP
    MP_WIFI_AUTH_WPA_PSK,           // WPA Personal
    MP_WIFI_AUTH_WPA2_PSK,          // WPA2 Personal
    MP_WIFI_AUTH_WPA_WPA2_PSK,      // WPA/WPA2 Personal
    MP_WIFI_AUTH_WPA2_ENTERPRISE,   // WPA2 Enterprise
    MP_WIFI_AUTH_WPA3_PSK,          // WPA3 Personal
    MP_WIFI_AUTH_WPA2_WPA3_PSK      // WPA2/WPA3 Personal
} mp_wifi_authmode_t;

// WiFi configuration
typedef struct {
    char ssid[32];           // SSID
    char password[64];       // Password
    uint8_t bssid[6];        // BSSID (MAC address)
    uint8_t channel;         // Channel (1-14)
    mp_wifi_authmode_t authmode; // Authentication mode
} mp_wifi_config_t;

// WiFi AP configuration
typedef struct {
    char ssid[32];           // SSID
    char password[64];       // Password (empty for open)
    uint8_t channel;         // Channel (1-14)
    mp_wifi_authmode_t authmode; // Authentication mode
    uint8_t max_connections; // Maximum number of connections (1-4)
    bool hidden;            // Whether SSID is hidden
} mp_wifi_ap_config_t;

// WiFi status
typedef struct {
    bool connected;          // Whether connected to a network
    int8_t rssi;             // Received signal strength (dBm)
    uint8_t channel;         // Channel
    uint8_t bssid[6];        // BSSID (MAC address)
    uint32_t ip_addr;        // IP address (host byte order)
    uint32_t netmask;        // Netmask (host byte order)
    uint32_t gateway;        // Gateway (host byte order)
} mp_wifi_status_t;

// WiFi AP information
typedef struct {
    char ssid[32];           // SSID
    int8_t rssi;             // Received signal strength (dBm)
    uint8_t channel;         // Channel
    mp_wifi_authmode_t authmode; // Authentication mode
    uint8_t bssid[6];        // BSSID (MAC address)
} mp_wifi_ap_info_t;

// WiFi event types
typedef enum {
    MP_WIFI_EVENT_CONNECTED,        // Connected to network
    MP_WIFI_EVENT_DISCONNECTED,     // Disconnected from network
    MP_WIFI_EVENT_GOT_IP,           // Got IP address
    MP_WIFI_EVENT_LOST_IP,          // Lost IP address
    MP_WIFI_EVENT_AP_STARTED,       // AP mode started
    MP_WIFI_EVENT_AP_STOPPED,       // AP mode stopped
    MP_WIFI_EVENT_AP_STA_CONNECTED, // Station connected to AP
    MP_WIFI_EVENT_AP_STA_DISCONNECTED, // Station disconnected from AP
    MP_WIFI_EVENT_SCAN_DONE         // Scan completed
} mp_wifi_event_type_t;

// WiFi event callback
typedef void (*mp_wifi_event_callback_t)(mp_wifi_event_type_t event);

// WiFi connection status callback
typedef void (*mp_wifi_conn_status_callback_t)(bool connected, mp_wifi_status_t *status);

// WiFi scan callback
typedef void (*mp_wifi_scan_callback_t)(void);

// WiFi Functions

/**
 * @brief Initialize WiFi driver
 * @return 0 on success, -1 on error
 */
int mp_wifi_init(void);

/**
 * @brief Deinitialize WiFi driver
 */
void mp_wifi_deinit(void);

/**
 * @brief Set WiFi configuration
 * @param config WiFi configuration
 */
void mp_wifi_set_config(mp_wifi_config_t *config);

/**
 * @brief Get WiFi configuration
 * @param config Output buffer for WiFi configuration
 */
void mp_wifi_get_config(mp_wifi_config_t *config);

/**
 * @brief Connect to WiFi network
 * @return 0 on success, -1 on error
 */
int mp_wifi_connect(void);

/**
 * @brief Disconnect from WiFi network
 */
void mp_wifi_disconnect(void);

/**
 * @brief Get current WiFi state
 * @return Current WiFi state
 */
mp_wifi_state_t mp_wifi_get_state(void);

/**
 * @brief Get WiFi connection status
 * @param status Output buffer for connection status
 */
void mp_wifi_get_status(mp_wifi_status_t *status);

/**
 * @brief Start WiFi access point (AP) mode
 * @param config AP configuration
 * @return 0 on success, -1 on error
 */
int mp_wifi_start_ap(mp_wifi_ap_config_t *config);

/**
 * @brief Stop WiFi access point (AP) mode
 */
void mp_wifi_stop_ap(void);

/**
 * @brief Start WiFi scan
 * @return 0 on success, -1 on error
 */
int mp_wifi_scan_start(void);

/**
 * @brief Get WiFi scan results
 * @param aps Output buffer for AP information
 * @param max_aps Maximum number of APs to return
 * @return Number of APs found, or -1 on error
 */
int mp_wifi_scan_get_results(mp_wifi_ap_info_t *aps, int max_aps);

/**
 * @brief Set WiFi event callback
 * @param callback Callback function for WiFi events
 */
void mp_wifi_set_event_callback(mp_wifi_event_callback_t callback);

/**
 * @brief Set WiFi connection status callback
 * @param callback Callback function for connection status changes
 */
void mp_wifi_set_conn_status_callback(mp_wifi_conn_status_callback_t callback);

/**
 * @brief Set WiFi scan callback
 * @param callback Callback function for scan completion
 */
void mp_wifi_set_scan_callback(mp_wifi_scan_callback_t callback);

// WiFi utility functions

/**
 * @brief Convert IP address to string
 * @param ip IP address in host byte order
 * @param str Output string buffer (must be at least 16 bytes)
 */
static inline void mp_wifi_ip_to_str(uint32_t ip, char *str) {
    snprintf(str, 16, "%d.%d.%d.%d",
             (ip >> 24) & 0xFF,
             (ip >> 16) & 0xFF,
             (ip >> 8) & 0xFF,
             ip & 0xFF);
}

/**
 * @brief Convert MAC address to string
 * @param mac MAC address (6 bytes)
 * @param str Output string buffer (must be at least 18 bytes)
 */
static inline void mp_wifi_mac_to_str(uint8_t *mac, char *str) {
    snprintf(str, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

#endif // MICROPOSIX_NETWORK_WIFI_H
