/**
 * microPOSIX I2C Driver for ESP32
 * 
 * This file implements a platform-agnostic I2C driver with ESP32-specific backend.
 * It provides a consistent API for I2C communication across different platforms.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "microposix/drivers/i2c.h"
#include "microposix/debug/log.h"
#include "microposix/kernel/thread.h"

// Platform detection
#if defined(MICROPOSIX_PLATFORM_ESP32)
#include "driver/i2c.h"
#include "esp_log.h"
#define I2C_BACKEND_ESP32 1
#elif defined(MICROPOSIX_PLATFORM_ARM)
// #include "stm32f4xx_hal.h"
#define I2C_BACKEND_ARM 1
#endif

// I2C device structure
typedef struct mp_i2c_device {
    uint8_t port;              // I2C port number
    uint8_t address;           // Device address (7-bit)
    uint32_t speed;            // Clock speed in Hz
    bool initialized;         // Whether the device is initialized
    void *platform_data;      // Platform-specific data
} mp_i2c_device_t;

// I2C port configuration
static mp_i2c_device_t i2c_devices[MP_I2C_MAX_DEVICES] = {0};

// Mutex for thread-safe access
static mp_mutex_t i2c_mutex;

/**
 * @brief Initialize I2C driver
 */
int mp_i2c_init(void) {
    MP_LOGI("I2C", "Initializing I2C driver");
    
    // Initialize mutex for thread safety
    // Note: In a real implementation, we'd use the actual mutex API
    // For now, we'll use a simple flag
    
    memset(i2c_devices, 0, sizeof(i2c_devices));
    
    #if defined(I2C_BACKEND_ESP32)
    // ESP32-specific initialization
    // I2C is initialized per-port, not globally
    #endif
    
    MP_LOGI("I2C", "I2C driver initialized");
    return 0;
}

/**
 * @brief Open an I2C device
 */
mp_i2c_handle_t mp_i2c_open(uint8_t port, uint32_t speed) {
    if (port >= MP_I2C_MAX_PORTS) {
        MP_LOGE("I2C", "Invalid I2C port: %d", port);
        return MP_I2C_INVALID_HANDLE;
    }
    
    // Find a free device slot
    for (int i = 0; i < MP_I2C_MAX_DEVICES; i++) {
        if (!i2c_devices[i].initialized) {
            i2c_devices[i].port = port;
            i2c_devices[i].speed = speed;
            i2c_devices[i].initialized = true;
            
            // Platform-specific initialization
            #if defined(I2C_BACKEND_ESP32)
            if (mp_i2c_esp32_init_port(port, speed) != 0) {
                i2c_devices[i].initialized = false;
                return MP_I2C_INVALID_HANDLE;
            }
            #endif
            
            MP_LOGI("I2C", "I2C port %d opened at %d Hz", port, speed);
            return (mp_i2c_handle_t)i;
        }
    }
    
    MP_LOGE("I2C", "No free I2C device slots");
    return MP_I2C_INVALID_HANDLE;
}

/**
 * @brief Close an I2C device
 */
void mp_i2c_close(mp_i2c_handle_t handle) {
    if (handle >= MP_I2C_MAX_DEVICES || !i2c_devices[handle].initialized) {
        return;
    }
    
    // Platform-specific deinitialization
    #if defined(I2C_BACKEND_ESP32)
    mp_i2c_esp32_deinit_port(i2c_devices[handle].port);
    #endif
    
    i2c_devices[handle].initialized = false;
    MP_LOGI("I2C", "I2C port %d closed", i2c_devices[handle].port);
}

/**
 * @brief Write data to an I2C device
 */
int mp_i2c_write(mp_i2c_handle_t handle, uint8_t address, const uint8_t *data, uint16_t len, uint32_t timeout_ms) {
    if (handle >= MP_I2C_MAX_DEVICES || !i2c_devices[handle].initialized) {
        MP_LOGE("I2C", "Invalid I2C handle: %d", handle);
        return -1;
    }
    
    if (data == NULL || len == 0) {
        MP_LOGE("I2C", "Invalid data or length");
        return -1;
    }
    
    #if defined(I2C_BACKEND_ESP32)
    return mp_i2c_esp32_write(i2c_devices[handle].port, address, data, len, timeout_ms);
    #else
    // Simulation for other platforms
    MP_LOGD("I2C", "Writing %d bytes to address 0x%02X on port %d", len, address, i2c_devices[handle].port);
    return len;
    #endif
}

/**
 * @brief Read data from an I2C device
 */
int mp_i2c_read(mp_i2c_handle_t handle, uint8_t address, uint8_t *data, uint16_t len, uint32_t timeout_ms) {
    if (handle >= MP_I2C_MAX_DEVICES || !i2c_devices[handle].initialized) {
        MP_LOGE("I2C", "Invalid I2C handle: %d", handle);
        return -1;
    }
    
    if (data == NULL || len == 0) {
        MP_LOGE("I2C", "Invalid data or length");
        return -1;
    }
    
    #if defined(I2C_BACKEND_ESP32)
    return mp_i2c_esp32_read(i2c_devices[handle].port, address, data, len, timeout_ms);
    #else
    // Simulation for other platforms
    MP_LOGD("I2C", "Reading %d bytes from address 0x%02X on port %d", len, address, i2c_devices[handle].port);
    memset(data, 0, len);  // Fill with zeros for simulation
    return len;
    #endif
}

/**
 * @brief Write then read from an I2C device (combined operation)
 */
int mp_i2c_write_read(mp_i2c_handle_t handle, uint8_t address, 
                      const uint8_t *write_data, uint16_t write_len,
                      uint8_t *read_data, uint16_t read_len, uint32_t timeout_ms) {
    if (handle >= MP_I2C_MAX_DEVICES || !i2c_devices[handle].initialized) {
        return -1;
    }
    
    if ((write_data == NULL && write_len > 0) || (read_data == NULL && read_len > 0)) {
        return -1;
    }
    
    #if defined(I2C_BACKEND_ESP32)
    return mp_i2c_esp32_write_read(i2c_devices[handle].port, address,
                                    write_data, write_len, read_data, read_len, timeout_ms);
    #else
    // Simulation
    if (write_len > 0) {
        MP_LOGD("I2C", "Writing %d bytes to address 0x%02X", write_len, address);
    }
    if (read_len > 0) {
        memset(read_data, 0, read_len);
        MP_LOGD("I2C", "Reading %d bytes from address 0x%02X", read_len, address);
    }
    return (write_len > 0 ? write_len : 0) + (read_len > 0 ? read_len : 0);
    #endif
}

/**
 * @brief Set I2C clock speed
 */
int mp_i2c_set_speed(mp_i2c_handle_t handle, uint32_t speed) {
    if (handle >= MP_I2C_MAX_DEVICES || !i2c_devices[handle].initialized) {
        return -1;
    }
    
    i2c_devices[handle].speed = speed;
    
    #if defined(I2C_BACKEND_ESP32)
    return mp_i2c_esp32_set_speed(i2c_devices[handle].port, speed);
    #else
    return 0;
    #endif
}

/**
 * @brief Get I2C clock speed
 */
uint32_t mp_i2c_get_speed(mp_i2c_handle_t handle) {
    if (handle >= MP_I2C_MAX_DEVICES || !i2c_devices[handle].initialized) {
        return 0;
    }
    
    return i2c_devices[handle].speed;
}

// ============================================================================
// ESP32-Specific Implementation
// ============================================================================

#if defined(I2C_BACKEND_ESP32)

static const char *TAG = "mp_i2c_esp32";

// I2C port configuration for ESP32
// ESP32 has two I2C controllers: I2C0 and I2C1
#define MP_I2C_ESP32_PORT_0 I2C_NUM_0
#define MP_I2C_ESP32_PORT_1 I2C_NUM_1

// Default I2C pins for ESP32
#define MP_I2C_ESP32_SDA_0 GPIO_NUM_21
#define MP_I2C_ESP32_SCL_0 GPIO_NUM_22
#define MP_I2C_ESP32_SDA_1 GPIO_NUM_19
#define MP_I2C_ESP32_SCL_1 GPIO_NUM_23

/**
 * @brief Initialize I2C port for ESP32
 */
int mp_i2c_esp32_init_port(uint8_t port, uint32_t speed) {
    if (port >= MP_I2C_MAX_PORTS) {
        ESP_LOGE(TAG, "Invalid I2C port: %d", port);
        return -1;
    }
    
    i2c_port_t i2c_port = (port == 0) ? MP_I2C_ESP32_PORT_0 : MP_I2C_ESP32_PORT_1;
    
    // Configure I2C
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = (port == 0) ? MP_I2C_ESP32_SDA_0 : MP_I2C_ESP32_SDA_1,
        .scl_io_num = (port == 0) ? MP_I2C_ESP32_SCL_0 : MP_I2C_ESP32_SCL_1,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = speed,
    };
    
    esp_err_t err = i2c_param_config(i2c_port, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure I2C %d: %s", port, esp_err_to_name(err));
        return -1;
    }
    
    err = i2c_driver_install(i2c_port, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install I2C %d driver: %s", port, esp_err_to_name(err));
        return -1;
    }
    
    ESP_LOGI(TAG, "I2C %d initialized at %d Hz", port, speed);
    return 0;
}

/**
 * @brief Deinitialize I2C port for ESP32
 */
void mp_i2c_esp32_deinit_port(uint8_t port) {
    if (port >= MP_I2C_MAX_PORTS) {
        return;
    }
    
    i2c_port_t i2c_port = (port == 0) ? MP_I2C_ESP32_PORT_0 : MP_I2C_ESP32_PORT_1;
    i2c_driver_delete(i2c_port);
    ESP_LOGI(TAG, "I2C %d deinitialized", port);
}

/**
 * @brief Write data to I2C device (ESP32)
 */
int mp_i2c_esp32_write(uint8_t port, uint8_t address, const uint8_t *data, uint16_t len, uint32_t timeout_ms) {
    if (port >= MP_I2C_MAX_PORTS) {
        return -1;
    }
    
    i2c_port_t i2c_port = (port == 0) ? MP_I2C_ESP32_PORT_0 : MP_I2C_ESP32_PORT_1;
    
    i2c_master_cmd_t cmd = {
        .cmd = 0,
        .addrs = {address << 1, I2C_MASTER_WRITE},
        .data = (uint8_t *)data,
        .data_len = len,
        .flags = 0
    };
    
    // Add write command
    cmd.cmd = I2C_MASTER_CMD_WRITE;
    
    esp_err_t err = i2c_master_cmd_begin(i2c_port, &cmd, pdMS_TO_TICKS(timeout_ms));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C write failed: %s", esp_err_to_name(err));
        return -1;
    }
    
    return len;
}

/**
 * @brief Read data from I2C device (ESP32)
 */
int mp_i2c_esp32_read(uint8_t port, uint8_t address, uint8_t *data, uint16_t len, uint32_t timeout_ms) {
    if (port >= MP_I2C_MAX_PORTS) {
        return -1;
    }
    
    i2c_port_t i2c_port = (port == 0) ? MP_I2C_ESP32_PORT_0 : MP_I2C_ESP32_PORT_1;
    
    i2c_master_cmd_t cmd = {
        .cmd = 0,
        .addrs = {address << 1, I2C_MASTER_READ},
        .data = data,
        .data_len = len,
        .flags = 0
    };
    
    // Add read command
    cmd.cmd = I2C_MASTER_CMD_READ;
    
    esp_err_t err = i2c_master_cmd_begin(i2c_port, &cmd, pdMS_TO_TICKS(timeout_ms));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C read failed: %s", esp_err_to_name(err));
        return -1;
    }
    
    return len;
}

/**
 * @brief Write then read from I2C device (ESP32)
 */
int mp_i2c_esp32_write_read(uint8_t port, uint8_t address,
                            const uint8_t *write_data, uint16_t write_len,
                            uint8_t *read_data, uint16_t read_len, uint32_t timeout_ms) {
    if (port >= MP_I2C_MAX_PORTS) {
        return -1;
    }
    
    i2c_port_t i2c_port = (port == 0) ? MP_I2C_ESP32_PORT_0 : MP_I2C_ESP32_PORT_1;
    
    if (write_len > 0 && read_len > 0) {
        // Combined write-then-read operation
        i2c_master_cmd_t cmd = {
            .cmd = 0,
            .addrs = {address << 1, I2C_MASTER_WRITE},
            .data = (uint8_t *)write_data,
            .data_len = write_len,
            .flags = 0
        };
        
        // Add write command
        cmd.cmd = I2C_MASTER_CMD_WRITE;
        
        esp_err_t err = i2c_master_cmd_begin(i2c_port, &cmd, pdMS_TO_TICKS(timeout_ms/2));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "I2C write failed: %s", esp_err_to_name(err));
            return -1;
        }
        
        // Now read
        cmd.addrs[1] = I2C_MASTER_READ;
        cmd.data = read_data;
        cmd.data_len = read_len;
        cmd.cmd = I2C_MASTER_CMD_READ;
        
        err = i2c_master_cmd_begin(i2c_port, &cmd, pdMS_TO_TICKS(timeout_ms/2));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "I2C read failed: %s", esp_err_to_name(err));
            return -1;
        }
        
        return write_len + read_len;
    } else if (write_len > 0) {
        return mp_i2c_esp32_write(port, address, write_data, write_len, timeout_ms);
    } else if (read_len > 0) {
        return mp_i2c_esp32_read(port, address, read_data, read_len, timeout_ms);
    }
    
    return 0;
}

/**
 * @brief Set I2C clock speed (ESP32)
 */
int mp_i2c_esp32_set_speed(uint8_t port, uint32_t speed) {
    if (port >= MP_I2C_MAX_PORTS) {
        return -1;
    }
    
    i2c_port_t i2c_port = (port == 0) ? MP_I2C_ESP32_PORT_0 : MP_I2C_ESP32_PORT_1;
    
    i2c_config_t conf;
    esp_err_t err = i2c_get_config(i2c_port, &conf);
    if (err != ESP_OK) {
        return -1;
    }
    
    conf.master.clk_speed = speed;
    err = i2c_param_config(i2c_port, &conf);
    if (err != ESP_OK) {
        return -1;
    }
    
    return 0;
}

#endif // I2C_BACKEND_ESP32
