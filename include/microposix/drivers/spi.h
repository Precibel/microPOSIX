#ifndef MICROPOSIX_DRIVERS_SPI_H
#define MICROPOSIX_DRIVERS_SPI_H

#include <stdint.h>
#include <stdbool.h>

// Maximum number of SPI devices
#define MP_SPI_MAX_DEVICES 8

// Maximum number of SPI buses
#define MP_SPI_MAX_BUSES 4

// Invalid SPI handle
#define MP_SPI_INVALID_HANDLE 0xFF

// SPI handle type
typedef uint8_t mp_spi_handle_t;

// SPI modes
#define MP_SPI_MODE_0 0  // CPOL=0, CPHA=0
#define MP_SPI_MODE_1 1  // CPOL=0, CPHA=1
#define MP_SPI_MODE_2 2  // CPOL=1, CPHA=0
#define MP_SPI_MODE_3 3  // CPOL=1, CPHA=1

// SPI standard speeds
#define MP_SPI_SPEED_125K 125000    // 125 kHz
#define MP_SPI_SPEED_250K 250000    // 250 kHz
#define MP_SPI_SPEED_500K 500000    // 500 kHz
#define MP_SPI_SPEED_1M 1000000    // 1 MHz
#define MP_SPI_SPEED_2M 2000000    // 2 MHz
#define MP_SPI_SPEED_4M 4000000    // 4 MHz
#define MP_SPI_SPEED_8M 8000000    // 8 MHz
#define MP_SPI_SPEED_10M 10000000  // 10 MHz
#define MP_SPI_SPEED_20M 20000000  // 20 MHz
#define MP_SPI_SPEED_40M 40000000  // 40 MHz

// SPI functions

/**
 * @brief Initialize SPI driver
 * @return 0 on success, -1 on error
 */
int mp_spi_init(void);

/**
 * @brief Open an SPI device
 * @param bus SPI bus number (0-3)
 * @param cs_pin Chip select pin
 * @param speed Clock speed in Hz
 * @param mode SPI mode (0-3)
 * @return SPI handle, or MP_SPI_INVALID_HANDLE on error
 */
mp_spi_handle_t mp_spi_open(uint8_t bus, uint8_t cs_pin, uint32_t speed, uint8_t mode);

/**
 * @brief Close an SPI device
 * @param handle SPI handle
 */
void mp_spi_close(mp_spi_handle_t handle);

/**
 * @brief Transfer data over SPI (full duplex)
 * @param handle SPI handle
 * @param tx_data Data to transmit (NULL if not transmitting)
 * @param rx_data Buffer to receive into (NULL if not receiving)
 * @param len Length of data to transfer
 * @param timeout_ms Timeout in milliseconds
 * @return Number of bytes transferred, or -1 on error
 */
int mp_spi_transfer(mp_spi_handle_t handle, const uint8_t *tx_data, uint8_t *rx_data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief Write data over SPI (transmit only)
 * @param handle SPI handle
 * @param data Data to transmit
 * @param len Length of data
 * @param timeout_ms Timeout in milliseconds
 * @return Number of bytes written, or -1 on error
 */
int mp_spi_write(mp_spi_handle_t handle, const uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief Read data over SPI (receive only)
 * @param handle SPI handle
 * @param data Buffer to receive into
 * @param len Length to read
 * @param timeout_ms Timeout in milliseconds
 * @return Number of bytes read, or -1 on error
 */
int mp_spi_read(mp_spi_handle_t handle, uint8_t *data, uint16_t len, uint32_t timeout_ms);

/**
 * @brief Set SPI clock speed
 * @param handle SPI handle
 * @param speed Clock speed in Hz
 * @return 0 on success, -1 on error
 */
int mp_spi_set_speed(mp_spi_handle_t handle, uint32_t speed);

/**
 * @brief Get SPI clock speed
 * @param handle SPI handle
 * @return Clock speed in Hz, or 0 on error
 */
uint32_t mp_spi_get_speed(mp_spi_handle_t handle);

/**
 * @brief Set SPI mode
 * @param handle SPI handle
 * @param mode SPI mode (0-3)
 * @return 0 on success, -1 on error
 */
int mp_spi_set_mode(mp_spi_handle_t handle, uint8_t mode);

/**
 * @brief Get SPI mode
 * @param handle SPI handle
 * @return SPI mode (0-3)
 */
uint8_t mp_spi_get_mode(mp_spi_handle_t handle);

/**
 * @brief Assert chip select (active low)
 * @param handle SPI handle
 */
void mp_spi_assert_cs(mp_spi_handle_t handle);

/**
 * @brief Deassert chip select (active high)
 * @param handle SPI handle
 */
void mp_spi_deassert_cs(mp_spi_handle_t handle);

#endif // MICROPOSIX_DRIVERS_SPI_H
