#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "microposix/kernel/abi.h"

// Macro to find the System Jump Table on CC2340R5
#define SYS_TABLE ((mp_syscall_table_t*) M0_SYS_TABLE_ADDR)

// App-side ABI Wrappers (these are normally compiled into libmicroposix.a)

// M33 SVC based (for CC2755)
void* app_malloc_m33(size_t size) {
    register uint32_t r0 __asm("r0") = size;
    __asm volatile ("svc %0" : "+r"(r0) : "i"(SYS_MALLOC) : "memory");
    return (void*)r0;
}

void app_sleep_m33(uint32_t ms) {
    register uint32_t r0 __asm("r0") = ms;
    __asm volatile ("svc %0" :: "i"(SYS_THREAD_SLEEP), "r"(r0) : "memory");
}

// M0+ Jump Table based (for CC2340R5)
void* app_malloc_m0(size_t size) {
    return SYS_TABLE->malloc(size);
}

void app_sleep_m0(uint32_t ms) {
    SYS_TABLE->thread_sleep(ms);
}

// Simple GPIO stubs (since we don't have full HAL yet)
void app_gpio_init(uint32_t pin, uint32_t mode) {
    // In real app, this would be an SVC or Jump Table call
}

void app_gpio_write(uint32_t pin, uint32_t level) {
    // In real app, this would be an SVC or Jump Table call
}

// THE USER APPLICATION (LED Blink)
// This is compiled into App Slot 0/1 at 0x00080000
void app_main(void) {
    // CC2755/CC2340R5 LED GPIO (Example)
    const uint32_t LED_RED = 6;
    
    app_gpio_init(LED_RED, 1); // Mode: Output
    
    while (1) {
        // Toggle LED
        app_gpio_write(LED_RED, 1);
        
        // Use ABI for sleep
#if defined(__ARM_ARCH_8M_MAIN__) // Cortex-M33
        app_sleep_m33(500);
#else // Cortex-M0+ fallback
        app_sleep_m0(500);
#endif
        
        app_gpio_write(LED_RED, 0);
        
#if defined(__ARM_ARCH_8M_MAIN__)
        app_sleep_m33(500);
#else
        app_sleep_m0(500);
#endif
    }
}

// Application Header (Placed at 0x00080000 via app.ld)
typedef struct {
    uint32_t magic;           // 0xAPP0BEEF 
    uint32_t version; 
    void (*app_entry)(void);  // Pointer to app_main 
} mp_app_header_t;

__attribute__((section(".app_header"), used))
const mp_app_header_t app_hdr = {
    .magic = 0xAPP0BEEF,
    .version = 0x010000,
    .app_entry = app_main
};
