/**
 * ESP32 ABI Initialization
 * 
 * This file initializes the ESP32-specific ABI jump table.
 * It maps the function pointers to the actual implementations.
 */

#include <stdint.h>
#include "microposix/kernel/abi_esp32.h"
#include "microposix/kernel/abi.h"
#include "microposix/mm/tlsf.h"
#include "microposix/kernel/thread.h"
#include "microposix/debug/log.h"
#include "microposix/hal/esp32/cpu.h"
#include "microposix/hal/esp32/gpio.h"
#include "microposix/hal/esp32/uart.h"
#include "microposix/hal/esp32/wdt.h"
#include "microposix/hal/esp32/mpu.h"
#include "microposix/ble/ble_esp32.h"

// External reference to the system table (defined in abi_esp32.h)
extern mp_syscall_table_esp32_t esp32_sys_table;

// TLSF heap instance (to be initialized)
static mp_tlsf_t tlsf_heap;
static uint8_t tlsf_pool[16384];  // 16KB TLSF pool for ESP32

/**
 * @brief Initialize the ESP32 ABI jump table
 * 
 * This function initializes all function pointers in the ESP32 system table
 * to point to the actual implementations.
 */
void mp_abi_esp32_init(void) {
    // Initialize TLSF heap
    mp_tlsf_init(&tlsf_heap, tlsf_pool, sizeof(tlsf_pool));
    
    // Base ABI functions
    esp32_sys_table.base.magic = MICROPOSIX_ABI_MAGIC;
    esp32_sys_table.base.abi_version = MICROPOSIX_ABI_VERSION;
    
    // Memory Management
    esp32_sys_table.base.malloc = (void* (*)(size_t))mp_tlsf_alloc;
    esp32_sys_table.base.free = (void  (*)(void*))mp_tlsf_free;
    
    // Thread Management
    esp32_sys_table.base.pthread_create = (int (*)(mp_pthread_t*, const mp_pthread_attr_t*, void *(*)(void*), void*))pthread_create;
    esp32_sys_table.base.pthread_join = (int (*)(mp_pthread_t, void**))pthread_join;
    esp32_sys_table.base.pthread_exit = (void (*)(void*))pthread_exit;
    esp32_sys_table.base.thread_sleep = (void (*)(uint32_t))mp_thread_sleep;
    
    // Hardware/Peripheral Access
    esp32_sys_table.base.gpio_init = (void (*)(uint32_t, uint32_t))mp_hal_esp32_gpio_init;
    esp32_sys_table.base.gpio_write = (void (*)(uint32_t, uint32_t))mp_hal_esp32_gpio_write;
    esp32_sys_table.base.gpio_read = (int  (*)(uint32_t))mp_hal_esp32_gpio_read;
    
    // BLE GATT
    esp32_sys_table.base.gatt_send = (int (*)(uint16_t, uint8_t*, uint16_t))mp_ble_esp32_send_data;
    
    // Debugging
    esp32_sys_table.base.log = (void (*)(int, const char*, const char*, ...))mp_log;
    
    // ESP32-specific functions
    esp32_sys_table.get_cycle_count = mp_hal_esp32_get_cycle_count;
    esp32_sys_table.get_core_id = mp_hal_esp32_get_core_id;
    esp32_sys_table.wfi = mp_hal_esp32_wfi;
    esp32_sys_table.reset = mp_hal_esp32_reset;
    
    // UART functions
    esp32_sys_table.uart_send = (int (*)(uint32_t, const uint8_t*, uint32_t))mp_hal_esp32_uart_send_data;
    esp32_sys_table.uart_receive = (int (*)(uint32_t, uint8_t*, uint32_t))mp_hal_esp32_uart_receive_data;
    
    // BLE functions
    esp32_sys_table.ble_init = mp_ble_esp32_init;
    esp32_sys_table.ble_send = (int (*)(uint16_t, uint8_t*, uint16_t))mp_ble_esp32_send_data;
    
    // Watchdog
    esp32_sys_table.wdt_feed = mp_hal_esp32_wdt_feed;
    
    // MPU/PMP
    esp32_sys_table.mpu_configure = (int (*)(uint32_t, uint32_t, uint32_t, uint32_t))mp_hal_esp32_mpu_configure_region;
    
    // ESP32-specific system info
    esp32_sys_table.get_chip_id = mp_hal_esp32_get_chip_id;
    esp32_sys_table.get_flash_size = NULL;  // To be implemented
    esp32_sys_table.get_ram_size = NULL;     // To be implemented
}

/**
 * @brief Get the ESP32 system table
 */
const mp_syscall_table_esp32_t *mp_abi_esp32_get_table(void) {
    return &esp32_sys_table;
}

/**
 * @brief ESP32-specific system call handler
 * 
 * For ESP32, we don't use SVC (Supervisor Call) as it's ARM-specific.
 * Instead, we use direct function calls through the jump table.
 * 
 * This function provides a fallback for any syscalls that might be
 * called through the assembly SVC handler (which shouldn't happen on ESP32).
 */
void mp_abi_esp32_syscall_handler(uint32_t syscall_id, uint32_t *args) {
    // This should never be called on ESP32
    // ESP32 uses direct function calls, not SVC
    
    switch (syscall_id) {
        case SYS_MALLOC:
            args[0] = (uint32_t)mp_tlsf_alloc(NULL, args[0]);
            break;
            
        case SYS_FREE:
            mp_tlsf_free(NULL, (void *)args[0]);
            break;
            
        case SYS_THREAD_SLEEP:
            mp_thread_sleep(args[0]);
            break;
            
        case SYS_GPIO_WRITE:
            mp_hal_esp32_gpio_write(args[0], args[1]);
            break;
            
        case SYS_GPIO_READ:
            args[0] = mp_hal_esp32_gpio_read(args[0]);
            break;
            
        case SYS_GATT_SEND:
            args[0] = mp_ble_esp32_send_data(args[0], (uint8_t *)args[1], args[2]);
            break;
            
        case SYS_LOG:
            // args[0] = level, args[1] = module, args[2] = fmt, args[3...] = varargs
            // This is tricky to handle in a syscall, so we skip it
            break;
            
        // ESP32-specific syscalls
        case SYS_ESP32_GET_CYCLE_COUNT:
            args[0] = mp_hal_esp32_get_cycle_count();
            break;
            
        case SYS_ESP32_GET_CORE_ID:
            args[0] = mp_hal_esp32_get_core_id();
            break;
            
        case SYS_ESP32_WFI:
            mp_hal_esp32_wfi();
            break;
            
        case SYS_ESP32_RESET:
            mp_hal_esp32_reset();
            break;
            
        case SYS_ESP32_BLE_INIT:
            args[0] = mp_ble_esp32_init();
            break;
            
        case SYS_ESP32_BLE_SEND:
            args[0] = mp_ble_esp32_send_data(args[0], (uint8_t *)args[1], args[2]);
            break;
            
        case SYS_ESP32_WDT_FEED:
            mp_hal_esp32_wdt_feed();
            break;
            
        default:
            // Unknown syscall
            break;
    }
}

/**
 * @brief Initialize ESP32-specific ABI components
 * 
 * This function should be called during system initialization
 * to set up the ESP32-specific ABI.
 */
void mp_abi_esp32_platform_init(void) {
    // Initialize the jump table
    mp_abi_esp32_init();
    
    // Initialize platform-specific components
    mp_hal_esp32_cpu_init();
    
    // Initialize BLE if enabled
    #ifdef CONFIG_BT_ENABLE
    mp_ble_esp32_init();
    #endif
}
