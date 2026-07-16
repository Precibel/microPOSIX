#ifndef MICROPOSIX_ABI_ESP32_H
#define MICROPOSIX_ABI_ESP32_H

#include <stdint.h>
#include <stddef.h>
#include "microposix/kernel/abi.h"
#include "microposix/kernel/thread.h"

/**
 * @brief ESP32-specific ABI extensions
 * 
 * This header extends the microPOSIX ABI for ESP32 platform.
 * Since ESP32 uses Xtensa or RISC-V architecture (not ARM), 
 * we need to adapt the ABI mechanism.
 * 
 * For ESP32, we use a function pointer table instead of SVC calls,
 * as SVC (Supervisor Call) is ARM-specific.
 */

// ESP32 ABI version (extends base ABI)
#define MICROPOSIX_ABI_ESP32_VERSION 0x01000001 // v1.0.1

// ESP32-specific syscall IDs (extend base syscall IDs)
#define SYS_ESP32_GET_CYCLE_COUNT  50
#define SYS_ESP32_GET_CORE_ID      51
#define SYS_ESP32_WFI              52
#define SYS_ESP32_RESET            53
#define SYS_ESP32_UART_SEND        54
#define SYS_ESP32_UART_RECEIVE     55
#define SYS_ESP32_BLE_INIT         56
#define SYS_ESP32_BLE_SEND         57
#define SYS_ESP32_WDT_FEED         58

// ESP32 System Jump Table structure
// This extends the base jump table with ESP32-specific functions
typedef struct {
    // Inherit base jump table
    mp_syscall_table_t base;
    
    // ESP32-specific functions
    uint32_t (*get_cycle_count)(void);
    int (*get_core_id)(void);
    void (*wfi)(void);
    void (*reset)(void);
    
    // UART functions
    int (*uart_send)(uint32_t port, const uint8_t *data, uint32_t len);
    int (*uart_receive)(uint32_t port, uint8_t *data, uint32_t max_len);
    
    // BLE functions
    int (*ble_init)(void);
    int (*ble_send)(uint16_t conn_handle, uint8_t *data, uint16_t len);
    
    // Watchdog
    void (*wdt_feed)(void);
    
    // MPU/PMP
    int (*mpu_configure)(uint32_t region_idx, uint32_t base_addr, uint32_t size, uint32_t permissions);
    
    // ESP32-specific
    uint32_t (*get_chip_id)(void);
    uint32_t (*get_flash_size)(void);
    uint32_t (*get_ram_size)(void);
    
} mp_syscall_table_esp32_t;

// System table address for ESP32
// ESP32 has different flash layout than ARM
// We'll place it at the end of the text segment
#if defined(CONFIG_IDF_TARGET_ESP32)
// ESP32 (Xtensa) - use IRAM for jump table
#define ESP32_SYS_TABLE_ADDR 0x3FFE0000
#elif defined(CONFIG_IDF_TARGET_ESP32S3) || defined(CONFIG_IDF_TARGET_ESP32C3)
// ESP32-S3/C3 (RISC-V) - use IRAM for jump table
#define ESP32_SYS_TABLE_ADDR 0x3FFE0000
#else
// Default for other platforms
#define ESP32_SYS_TABLE_ADDR 0x0003FE00
#endif

// Define the system table for ESP32
__attribute__((section(".system_table")))
const mp_syscall_table_esp32_t esp32_sys_table = {
    .base = {
        .magic = MICROPOSIX_ABI_MAGIC,
        .abi_version = MICROPOSIX_ABI_VERSION,
        
        // Memory Management
        .malloc = NULL,  // Will be initialized at runtime
        .free = NULL,
        
        // Thread Management
        .pthread_create = NULL,
        .pthread_join = NULL,
        .pthread_exit = NULL,
        .thread_sleep = NULL,
        
        // Hardware/Peripheral Access
        .gpio_init = NULL,
        .gpio_write = NULL,
        .gpio_read = NULL,
        
        // BLE GATT
        .gatt_send = NULL,
        
        // Debugging
        .log = NULL
    },
    
    // ESP32-specific functions
    .get_cycle_count = NULL,
    .get_core_id = NULL,
    .wfi = NULL,
    .reset = NULL,
    .uart_send = NULL,
    .uart_receive = NULL,
    .ble_init = NULL,
    .ble_send = NULL,
    .wdt_feed = NULL,
    .mpu_configure = NULL,
    .get_chip_id = NULL,
    .get_flash_size = NULL,
    .get_ram_size = NULL
};

// Initialize the ESP32 system table
void mp_abi_esp32_init(void);

// Get the ESP32 system table
const mp_syscall_table_esp32_t *mp_abi_esp32_get_table(void);

// ESP32 ABI compatibility macros
// These macros provide a unified interface across all platforms

// For ESP32, we use direct function calls (no SVC)
#define mp_abi_malloc(size) (mp_abi_esp32_get_table()->base.malloc ? \
    mp_abi_esp32_get_table()->base.malloc(size) : NULL)

define mp_abi_free(ptr) (mp_abi_esp32_get_table()->base.free ? \
    mp_abi_esp32_get_table()->base.free(ptr) : (void)0)

#define mp_abi_thread_sleep(ms) (mp_abi_esp32_get_table()->base.thread_sleep ? \
    mp_abi_esp32_get_table()->base.thread_sleep(ms) : (void)0)

#define mp_abi_gpio_write(pin, level) (mp_abi_esp32_get_table()->base.gpio_write ? \
    mp_abi_esp32_get_table()->base.gpio_write(pin, level) : (void)0)

#define mp_abi_gpio_read(pin) (mp_abi_esp32_get_table()->base.gpio_read ? \
    mp_abi_esp32_get_table()->base.gpio_read(pin) : 0)

// ESP32-specific ABI macros
#define mp_abi_esp32_get_cycle_count() (mp_abi_esp32_get_table()->get_cycle_count ? \
    mp_abi_esp32_get_table()->get_cycle_count() : 0)

#define mp_abi_esp32_get_core_id() (mp_abi_esp32_get_table()->get_core_id ? \
    mp_abi_esp32_get_table()->get_core_id() : 0)

#define mp_abi_esp32_wfi() (mp_abi_esp32_get_table()->wfi ? \
    mp_abi_esp32_get_table()->wfi() : (void)0)

#define mp_abi_esp32_reset() (mp_abi_esp32_get_table()->reset ? \
    mp_abi_esp32_get_table()->reset() : (void)0)

#define mp_abi_esp32_ble_init() (mp_abi_esp32_get_table()->ble_init ? \
    mp_abi_esp32_get_table()->ble_init() : -1)

#define mp_abi_esp32_ble_send(handle, data, len) (mp_abi_esp32_get_table()->ble_send ? \
    mp_abi_esp32_get_table()->ble_send(handle, data, len) : -1)

#define mp_abi_esp32_wdt_feed() (mp_abi_esp32_get_table()->wdt_feed ? \
    mp_abi_esp32_get_table()->wdt_feed() : (void)0)

#endif // MICROPOSIX_ABI_ESP32_H
