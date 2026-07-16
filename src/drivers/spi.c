/**
 * microPOSIX SPI Driver for ESP32
 * 
 * This file implements a platform-agnostic SPI driver with ESP32-specific backend.
 * It provides a consistent API for SPI communication across different platforms.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "microposix/drivers/spi.h"
#include "microposix/debug/log.h"
#include "microposix/kernel/thread.h"

// Platform detection
#if defined(MICROPOSIX_PLATFORM_ESP32)
#include "driver/spi_master.h"
#include "esp_log.h"
#define SPI_BACKEND_ESP32 1
#elif defined(MICROPOSIX_PLATFORM_ARM)
// #include "stm32f4xx_hal.h"
#define SPI_BACKEND_ARM 1
#endif

// SPI device structure
typedef struct mp_spi_device {
    uint8_t bus;               // SPI bus number
    uint8_t cs_pin;            // Chip select pin
    uint32_t speed;            // Clock speed in Hz
    uint8_t mode;              // SPI mode (0-3)
    bool initialized;         // Whether the device is initialized
    void *platform_data;      // Platform-specific data
} mp_spi_device_t;

// SPI bus configuration
static mp_spi_device_t spi_devices[MP_SPI_MAX_DEVICES] = {0};

/**
 * @brief Initialize SPI driver
 */
int mp_spi_init(void) {
    MP_LOGI("SPI", "Initializing SPI driver");
    
    memset(spi_devices, 0, sizeof(spi_devices));
    
    #if defined(SPI_BACKEND_ESP32)
    // ESP32-specific initialization
    // SPI is initialized per-bus, not globally
    #endif
    
    MP_LOGI("SPI", "SPI driver initialized");
    return 0;
}

/**
 * @brief Open an SPI device
 */
mp_spi_handle_t mp_spi_open(uint8_t bus, uint8_t cs_pin, uint32_t speed, uint8_t mode) {
    if (bus >= MP_SPI_MAX_BUSES) {
        MP_LOGE("SPI", "Invalid SPI bus: %d", bus);
        return MP_SPI_INVALID_HANDLE;
    }
    
    // Find a free device slot
    for (int i = 0; i < MP_SPI_MAX_DEVICES; i++) {
        if (!spi_devices[i].initialized) {
            spi_devices[i].bus = bus;
            spi_devices[i].cs_pin = cs_pin;
            spi_devices[i].speed = speed;
            spi_devices[i].mode = mode & 0x03;  // Only 2 bits for mode
            spi_devices[i].initialized = true;
            
            // Platform-specific initialization
            #if defined(SPI_BACKEND_ESP32)
            if (mp_spi_esp32_init_bus(bus, speed, mode) != 0) {
                spi_devices[i].initialized = false;
                return MP_SPI_INVALID_HANDLE;
            }
            #endif
            
            MP_LOGI("SPI", "SPI bus %d opened, CS: %d, speed: %d Hz, mode: %d", 
                   bus, cs_pin, speed, mode);
            return (mp_spi_handle_t)i;
        }
    }
    
    MP_LOGE("SPI", "No free SPI device slots");
    return MP_SPI_INVALID_HANDLE;
}

/**
 * @brief Close an SPI device
 */
void mp_spi_close(mp_spi_handle_t handle) {
    if (handle >= MP_SPI_MAX_DEVICES || !spi_devices[handle].initialized) {
        return;
    }
    
    // Platform-specific deinitialization
    #if defined(SPI_BACKEND_ESP32)
    mp_spi_esp32_deinit_bus(spi_devices[handle].bus);
    #endif
    
    spi_devices[handle].initialized = false;
    MP_LOGI("SPI", "SPI bus %d closed", spi_devices[handle].bus);
}

/**
 * @brief Transfer data over SPI (full duplex)
 */
int mp_spi_transfer(mp_spi_handle_t handle, const uint8_t *tx_data, uint8_t *rx_data, uint16_t len, uint32_t timeout_ms) {
    if (handle >= MP_SPI_MAX_DEVICES || !spi_devices[handle].initialized) {
        MP_LOGE("SPI", "Invalid SPI handle: %d", handle);
        return -1;
    }
    
    if (len == 0) {
        return 0;
    }
    
    if (tx_data == NULL && rx_data == NULL) {
        MP_LOGE("SPI", "Both tx_data and rx_data are NULL");
        return -1;
    }
    
    #if defined(SPI_BACKEND_ESP32)
    return mp_spi_esp32_transfer(spi_devices[handle].bus, spi_devices[handle].cs_pin,
                                  tx_data, rx_data, len, timeout_ms);
    #else
    // Simulation for other platforms
    if (rx_data != NULL) {
        memset(rx_data, 0, len);  // Fill with zeros for simulation
    }
    MP_LOGD("SPI", "Transferred %d bytes on bus %d", len, spi_devices[handle].bus);
    return len;
    #endif
}

/**
 * @brief Write data over SPI (transmit only)
 */
int mp_spi_write(mp_spi_handle_t handle, const uint8_t *data, uint16_t len, uint32_t timeout_ms) {
    return mp_spi_transfer(handle, data, NULL, len, timeout_ms);
}

/**
 * @brief Read data over SPI (receive only)
 */
int mp_spi_read(mp_spi_handle_t handle, uint8_t *data, uint16_t len, uint32_t timeout_ms) {
    return mp_spi_transfer(handle, NULL, data, len, timeout_ms);
}

/**
 * @brief Set SPI clock speed
 */
int mp_spi_set_speed(mp_spi_handle_t handle, uint32_t speed) {
    if (handle >= MP_SPI_MAX_DEVICES || !spi_devices[handle].initialized) {
        return -1;
    }
    
    spi_devices[handle].speed = speed;
    
    #if defined(SPI_BACKEND_ESP32)
    return mp_spi_esp32_set_speed(spi_devices[handle].bus, speed);
    #else
    return 0;
    #endif
}

/**
 * @brief Get SPI clock speed
 */
uint32_t mp_spi_get_speed(mp_spi_handle_t handle) {
    if (handle >= MP_SPI_MAX_DEVICES || !spi_devices[handle].initialized) {
        return 0;
    }
    
    return spi_devices[handle].speed;
}

/**
 * @brief Set SPI mode
 */
int mp_spi_set_mode(mp_spi_handle_t handle, uint8_t mode) {
    if (handle >= MP_SPI_MAX_DEVICES || !spi_devices[handle].initialized) {
        return -1;
    }
    
    spi_devices[handle].mode = mode & 0x03;
    
    #if defined(SPI_BACKEND_ESP32)
    return mp_spi_esp32_set_mode(spi_devices[handle].bus, mode);
    #else
    return 0;
    #endif
}

/**
 * @brief Get SPI mode
 */
uint8_t mp_spi_get_mode(mp_spi_handle_t handle) {
    if (handle >= MP_SPI_MAX_DEVICES || !spi_devices[handle].initialized) {
        return 0;
    }
    
    return spi_devices[handle].mode;
}

/**
 * @brief Assert chip select (active low)
 */
void mp_spi_assert_cs(mp_spi_handle_t handle) {
    if (handle >= MP_SPI_MAX_DEVICES || !spi_devices[handle].initialized) {
        return;
    }
    
    #if defined(SPI_BACKEND_ESP32)
    mp_spi_esp32_assert_cs(spi_devices[handle].bus, spi_devices[handle].cs_pin);
    #else
    // Simulation
    MP_LOGD("SPI", "Asserting CS on pin %d", spi_devices[handle].cs_pin);
    #endif
}

/**
 * @brief Deassert chip select (active high)
 */
void mp_spi_deassert_cs(mp_spi_handle_t handle) {
    if (handle >= MP_SPI_MAX_DEVICES || !spi_devices[handle].initialized) {
        return;
    }
    
    #if defined(SPI_BACKEND_ESP32)
    mp_spi_esp32_deassert_cs(spi_devices[handle].bus, spi_devices[handle].cs_pin);
    #else
    // Simulation
    MP_LOGD("SPI", "Deasserting CS on pin %d", spi_devices[handle].cs_pin);
    #endif
}

// ============================================================================
// ESP32-Specific Implementation
// ============================================================================

#if defined(SPI_BACKEND_ESP32)

static const char *TAG = "mp_spi_esp32";

// SPI bus configuration for ESP32
// ESP32 has 4 SPI controllers: SPI0, SPI1, SPI2, SPI3
// SPI0 and SPI1 are available for general use
#define MP_SPI_ESP32_BUS_0 SPI2_HOST  // HSPI
#define MP_SPI_ESP32_BUS_1 SPI3_HOST  // VSPI

// Default SPI pins for ESP32
// HSPI (SPI2)
#define MP_SPI_ESP32_MOSI_0 GPIO_NUM_13
#define MP_SPI_ESP32_MISO_0 GPIO_NUM_12
#define MP_SPI_ESP32_SCLK_0 GPIO_NUM_14
#define MP_SPI_ESP32_CS_0 GPIO_NUM_15

// VSPI (SPI3)
#define MP_SPI_ESP32_MOSI_1 GPIO_NUM_23
#define MP_SPI_ESP32_MISO_1 GPIO_NUM_19
#define MP_SPI_ESP32_SCLK_1 GPIO_NUM_18
#define MP_SPI_ESP32_CS_1 GPIO_NUM_5

// SPI transaction structure for ESP32
static spi_device_handle_t spi_handles[MP_SPI_MAX_BUSES] = {0};

/**
 * @brief Initialize SPI bus for ESP32
 */
int mp_spi_esp32_init_bus(uint8_t bus, uint32_t speed, uint8_t mode) {
    if (bus >= MP_SPI_MAX_BUSES) {
        ESP_LOGE(TAG, "Invalid SPI bus: %d", bus);
        return -1;
    }
    
    spi_bus_config_t buscfg = {
        .miso_io_num = (bus == 0) ? MP_SPI_ESP32_MISO_0 : MP_SPI_ESP32_MISO_1,
        .mosi_io_num = (bus == 0) ? MP_SPI_ESP32_MOSI_0 : MP_SPI_ESP32_MOSI_1,
        .sclk_io_num = (bus == 0) ? MP_SPI_ESP32_SCLK_0 : MP_SPI_ESP32_SCLK_1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 32,
    };
    
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = speed,
        .mode = mode,
        .spics_io_num = -1,  // We'll handle CS manually
        .queue_size = 7,
    };
    
    spi_host_device_t host = (bus == 0) ? MP_SPI_ESP32_BUS_0 : MP_SPI_ESP32_BUS_1;
    
    esp_err_t err = spi_bus_initialize(host, &buscfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus %d: %s", bus, esp_err_to_name(err));
        return -1;
    }
    
    // Add device to bus (without CS pin, we'll handle it manually)
    err = spi_bus_add_device(host, &devcfg, &spi_handles[bus]);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device %d: %s", bus, esp_err_to_name(err));
        spi_bus_free(host);
        return -1;
    }
    
    ESP_LOGI(TAG, "SPI bus %d initialized at %d Hz, mode %d", bus, speed, mode);
    return 0;
}

/**
 * @brief Deinitialize SPI bus for ESP32
 */
void mp_spi_esp32_deinit_bus(uint8_t bus) {
    if (bus >= MP_SPI_MAX_BUSES) {
        return;
    }
    
    spi_host_device_t host = (bus == 0) ? MP_SPI_ESP32_BUS_0 : MP_SPI_ESP32_BUS_1;
    
    if (spi_handles[bus] != NULL) {
        spi_bus_remove_device(spi_handles[bus]);
        spi_handles[bus] = NULL;
    }
    
    spi_bus_free(host);
    ESP_LOGI(TAG, "SPI bus %d deinitialized", bus);
}

/**
 * @brief Transfer data over SPI (ESP32)
 */
int mp_spi_esp32_transfer(uint8_t bus, uint8_t cs_pin, const uint8_t *tx_data, 
                         uint8_t *rx_data, uint16_t len, uint32_t timeout_ms) {
    if (bus >= MP_SPI_MAX_BUSES) {
        return -1;
    }
    
    spi_host_device_t host = (bus == 0) ? MP_SPI_ESP32_BUS_0 : MP_SPI_ESP32_BUS_1;
    
    // Create transaction
    spi_transaction_t t = {
        .length = len * 8,  // Length in bits
        .tx_buffer = (void *)tx_data,
        .rx_buffer = (void *)rx_data,
    };
    
    // Assert CS
    gpio_set_level(cs_pin, 0);
    
    // Perform transaction
    esp_err_t err = spi_device_transmit(spi_handles[bus], &t);
    
    // Deassert CS
    gpio_set_level(cs_pin, 1);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI transfer failed: %s", esp_err_to_name(err));
        return -1;
    }
    
    return len;
}

/**
 * @brief Set SPI clock speed (ESP32)
 */
int mp_spi_esp32_set_speed(uint8_t bus, uint32_t speed) {
    if (bus >= MP_SPI_MAX_BUSES || spi_handles[bus] == NULL) {
        return -1;
    }
    
    spi_device_interface_config_t devcfg;
    esp_err_t err = spi_bus_get_device_config(spi_handles[bus], &devcfg);
    if (err != ESP_OK) {
        return -1;
    }
    
    devcfg.clock_speed_hz = speed;
    err = spi_bus_update_device_config(spi_handles[bus], &devcfg);
    if (err != ESP_OK) {
        return -1;
    }
    
    return 0;
}

/**
 * @brief Set SPI mode (ESP32)
 */
int mp_spi_esp32_set_mode(uint8_t bus, uint8_t mode) {
    if (bus >= MP_SPI_MAX_BUSES || spi_handles[bus] == NULL) {
        return -1;
    }
    
    spi_device_interface_config_t devcfg;
    esp_err_t err = spi_bus_get_device_config(spi_handles[bus], &devcfg);
    if (err != ESP_OK) {
        return -1;
    }
    
    devcfg.mode = mode;
    err = spi_bus_update_device_config(spi_handles[bus], &devcfg);
    if (err != ESP_OK) {
        return -1;
    }
    
    return 0;
}

/**
 * @brief Assert chip select (ESP32)
 */
void mp_spi_esp32_assert_cs(uint8_t bus, uint8_t cs_pin) {
    (void)bus;  // bus is not used, we handle CS manually
    gpio_set_level(cs_pin, 0);
}

/**
 * @brief Deassert chip select (ESP32)
 */
void mp_spi_esp32_deassert_cs(uint8_t bus, uint8_t cs_pin) {
    (void)bus;  // bus is not used, we handle CS manually
    gpio_set_level(cs_pin, 1);
}

#endif // SPI_BACKEND_ESP32
