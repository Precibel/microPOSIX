#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "microposix/kernel/scheduler.h"
#include "microposix/kernel/thread.h"
#include "microposix/debug/log.h"
#include "microposix/debug/shell.h"
#include "microposix/bootloader/boot.h"
#include "pthread.h"

// Resident OS Application Header Definition
typedef struct {
    uint32_t magic;           // e.g., 0xAPP0BEEF 
    uint32_t version; 
    void (*app_entry)(void);  // Pointer to app_main 
} mp_app_header_t;

// Simulated boot info (normally in flash or retained SRAM)
static mp_boot_info_t boot_info = {
    .magic = 0x55AA55AA,
    .version = 1,
    .active_slot = 0 // Default to Slot A
};

// Mutex for protecting shared resources
mp_pthread_mutex_t log_mutex;

// Thread 1 function
void *thread1_func(void *arg) {
    (void)arg;
    while (1) {
        pthread_mutex_lock(&log_mutex);
        MP_LOGI("APP", "Thread 1 is running...");
        pthread_mutex_unlock(&log_mutex);
        mp_thread_sleep(1000); // Sleep for 1 second
    }
    return NULL;
}

// Thread 2 function
void *thread2_func(void *arg) {
    (void)arg;
    while (1) {
        pthread_mutex_lock(&log_mutex);
        MP_LOGI("APP", "Thread 2 is running...");
        pthread_mutex_unlock(&log_mutex);
        mp_thread_sleep(2000); // Sleep for 2 seconds
    }
    return NULL;
}

int main(void) {
    // 1. Bootloader Entry
    // Check for UART update request (e.g. Pin low or special character)
    // if (mp_boot_check_update_request()) {
    //     mp_boot_uart_update();
    // }

    // 2. Application Selection & Validation
    uint32_t app_addr = (boot_info.active_slot == 0) ? SLOT_A_START : SLOT_B_START;
    mp_app_header_t *app_hdr = (mp_app_header_t *)app_addr;

    if (app_hdr->magic == 0xAPP0BEEF) {
        printf("Bootloader: Valid App found at 0x%08X (v%d)\n", app_addr, app_hdr->version);
        // In a real resident kernel, we would create a thread for app_hdr->app_entry
    } else {
        printf("Bootloader: No valid App at 0x%08X. Falling back to OS Shell.\n", app_addr);
    }

    // 3. Initialize OS components
    mp_sched_init();
    mp_shell_init();
    
    // Initialize mutex
    pthread_mutex_init(&log_mutex, NULL);
    
    // Start Resident OS Shell
    mp_shell_start();
    
    // Start the scheduler
    mp_sched_start();
    
    // Should not reach here
    while (1);
    
    return 0;
}
