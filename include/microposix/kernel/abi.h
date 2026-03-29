#ifndef MICROPOSIX_ABI_H
#define MICROPOSIX_ABI_H

#include <stdint.h>
#include <stddef.h>
#include "microposix/kernel/thread.h"
#include "pthread.h"

// ABI Versioning
#define MICROPOSIX_ABI_MAGIC   0xDEADBEEF
#define MICROPOSIX_ABI_VERSION 0x01000000 // v1.0.0

// Syscall IDs for Cortex-M33 SVC Router
#define SYS_MALLOC          1
#define SYS_FREE            2
#define SYS_PTHREAD_CREATE  3
#define SYS_PTHREAD_JOIN    4
#define SYS_PTHREAD_EXIT    5
#define SYS_THREAD_SLEEP    6
#define SYS_LOG             7
#define SYS_GPIO_INIT       8
#define SYS_GPIO_WRITE      9
#define SYS_GPIO_READ       10
#define SYS_GATT_SEND       11

// System Jump Table structure for Cortex-M0+ (CC2340R5)
typedef struct {
    uint32_t magic;
    uint32_t abi_version;
    
    // Memory Management
    void* (*malloc)(size_t);
    void  (*free)(void*);
    
    // Thread Management
    int (*pthread_create)(mp_pthread_t*, const mp_pthread_attr_t*, void *(*)(void*), void*);
    int (*pthread_join)(mp_pthread_t, void**);
    void (*pthread_exit)(void*);
    void (*thread_sleep)(uint32_t);
    
    // Hardware/Peripheral Access
    void (*gpio_init)(uint32_t pin, uint32_t mode);
    void (*gpio_write)(uint32_t pin, uint32_t level);
    int  (*gpio_read)(uint32_t pin);
    
    // BLE GATT
    int (*gatt_send)(uint16_t conn_handle, uint8_t *data, uint16_t len);
    
    // Debugging
    void (*log)(int level, const char *module, const char *fmt, ...);
} mp_syscall_table_t;

// Standard location for Jump Table (Last 512 bytes of Resident OS Flash)
#if defined(__ARM_ARCH_8M_MAIN__) // Cortex-M33 (CC2755)
#define M0_SYS_TABLE_ADDR 0x00077E00
#else // Cortex-M0+ (CC2340R5)
#define M0_SYS_TABLE_ADDR 0x0003FE00
#endif

#endif // MICROPOSIX_ABI_H
