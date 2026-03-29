#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>
#include "microposix/debug/log.h"
#include "microposix/kernel/scheduler.h"
#include "microposix/kernel/thread.h"

// Ring buffer for logging
#define LOG_BUFFER_SIZE 4096
static uint8_t log_buffer[LOG_BUFFER_SIZE];
static uint32_t log_head = 0;
static uint32_t log_tail = 0;

static const char *level_names[] = {
    "VRB", "DBG", "INF", "WRN", "ERR", "CRT"
};

void mp_log(mp_log_level_t level, const char *module, const char *fmt, ...) {
    char entry[256];
    int len;
    
    // Get current thread name
    mp_tcb_t *current = mp_sched_get_current_thread();
    const char *thread_name = current ? current->name : "system";
    
    // Format the log entry
    // Format: [timestamp] [level] [thread] [module] message
    len = snprintf(entry, sizeof(entry), "[%s] [%s] [%s] ", 
                   level_names[level], thread_name, module);
    
    va_list args;
    va_start(args, fmt);
    len += vsnprintf(entry + len, sizeof(entry) - len - 2, fmt, args);
    va_end(args);
    
    // Add newline
    entry[len++] = '\r';
    entry[len++] = '\n';
    
    // Copy to ring buffer (simplified, not thread-safe yet)
    // In a real implementation, use a lock-free ring buffer or a mutex
    for (int i = 0; i < len; i++) {
        log_buffer[log_head] = entry[i];
        log_head = (log_head + 1) % LOG_BUFFER_SIZE;
    }
    
    // In a real implementation, trigger UART DMA if not already running
}

// Function to drain the log buffer (to be called from a low-priority thread)
void mp_log_drain(void) {
    while (log_tail != log_head) {
        // Send byte to UART0
        // mp_hal_uart_send_byte(UART0, log_buffer[log_tail]);
        log_tail = (log_tail + 1) % LOG_BUFFER_SIZE;
    }
}
