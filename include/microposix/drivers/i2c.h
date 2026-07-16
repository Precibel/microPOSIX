#ifndef MICROPOSIX_DRIVERS_I2C_H
#define MICROPOSIX_DRIVERS_I2C_H

#include <stdint.h>
#include <stdbool.h>

// Maximum number of I2C devices
#define MP_I2C_MAX_DEVICES 8

// Maximum number of I2C ports
#define MP_I2C_MAX_PORTS 2

// Invalid I2C handle
#define MP_I2C_INVALID_HANDLE 0xFF

// I2C handle type
typedef uint8_t mp_i2c_handle_t;

// I2C standard speeds
#define MP_I2C_SPEED_STANDARD 100000   // 100 kHz
#define MP_I2C_SPEED_FAST 400000       // 400 kHz
#define MP_I2C_SPEED_FAST_PLUS 1000000 // 1 MHz
#define MP_I2C_SPEED_HIGH 3400000     // 3.4 MHz

// I2C functions

/**
 * @brief Initialize I2C driver
 * @return 0 on success, -1 on error
 */
int mp_i2c_init(void);

/**
 * @brief Open an I2C device
 * @param port I2C port number (0 or 1)
 * @param speed Clock speed in Hz
 * @return I2C handle, or MP_I2C_INVALID_HANDLE on error
 */
mp_i2c_handle_t mp_i2c_open(uint8_t port, uint32_t speed);

/**
 * @brief Close an I2C device
 * @param handle I2C handle
 */
void mp_i2c_close(mp_i2c_handle_t handle);

/**
 * @brief Write data to an I2C device
 * @param handle I2C handle
 * @param address Device address (7-bit)
 * @param data Data to write
 * @param len Length of data
 * @param timeout_ms Timeout in milliseconds
 * @return Number of bytes written, or -1 on error
 */
int mp_i2c_write(mp_i2c_handle_t handle, uint8_t address, const uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief Read data from an I2C device
 * @param handle I2C handle
 * @param address Device address (7-bit)
 * @param data Buffer to read into
 * @param len Length to read
 * @param timeout_ms Timeout in milliseconds
 * @return Number of bytes read, or -1 on error
 */
int mp_i2c_read(mp_i2c_handle_t handle, uint8_t address, uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief Write then read from an I2C device (combined operation)
 * @param handle I2C handle
 * @param address Device address (7-bit)
 * @param write_data Data to write
 * @param write_len Length of data to write
 * @param read_data Buffer to read into
 * @param read_len Length to read
 * @param timeout_ms Timeout in milliseconds
 * @return Number of bytes transferred (write_len + read_len), or -1 on error
 */
int mp_i2c_write_read(mp_i2c_handle_t handle, uint8_t address,
                      const uint8_t *write_data, uint16_t write_len,
                      uint8_t *read_data, uint16_t read_len, uint32_t timeout_ms);

/**
 * @brief Set I2C clock speed
 * @param handle I2C handle
 * @param speed Clock speed in Hz
 * @return 0 on success, -1 on error
 */
int mp_i2c_set_speed(mp_i2c_handle_t handle, uint32_t speed);

/**
 * @brief Get I2C clock speed
 * @param handle I2C handle
 * @return Clock speed in Hz, or 0 on error
 */
uint32_t mp_i2c_get_speed(mp_i2c_handle_t handle);

#endif // MICROPOSIX_DRIVERS_I2C_H
