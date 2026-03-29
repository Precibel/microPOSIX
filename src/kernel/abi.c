#include <stdint.h>
#include <stdbool.h>
#include "microposix/kernel/abi.h"
#include "microposix/kernel/scheduler.h"
#include "microposix/mm/tlsf.h"
#include "microposix/debug/log.h"

// System-wide Jump Table definition (M0+ specific)
__attribute__((section(".system_table")))
const mp_syscall_table_t sys_table = {
    .magic = MICROPOSIX_ABI_MAGIC,
    .abi_version = MICROPOSIX_ABI_VERSION,
    
    // Memory
    .malloc = (void* (*)(size_t))mp_tlsf_alloc, // Simplified mapping
    .free   = (void  (*)(void*))mp_tlsf_free,
    
    // Threads
    .pthread_create = (int (*)(mp_pthread_t*, const mp_pthread_attr_t*, void *(*)(void*), void*))pthread_create,
    .pthread_join   = (int (*)(mp_pthread_t, void**))pthread_join,
    .pthread_exit   = (void (*)(void*))pthread_exit,
    .thread_sleep   = (void (*)(uint32_t))mp_thread_sleep,
    
    // Hardware (Stubs for now)
    .gpio_init  = NULL, 
    .gpio_write = NULL,
    .gpio_read  = NULL,
    
    // Debugging
    .log = (void (*)(int, const char*, const char*, ...))mp_log
};

// SVC Router for Cortex-M33 (CC2755)
// This C function is called from assembly SVC_Handler
void mp_kernel_svc_router(uint32_t svc_id, uint32_t *args) {
    switch (svc_id) {
        case SYS_MALLOC:
            args[0] = (uint32_t)mp_tlsf_alloc(NULL, args[0]); // Simplified
            break;
            
        case SYS_THREAD_SLEEP:
            mp_thread_sleep(args[0]);
            break;
            
        case SYS_LOG:
            // Extract arguments from caller stack frame and call mp_log
            break;
            
        default:
            // Unknown syscall
            break;
    }
}
