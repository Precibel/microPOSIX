#ifndef MICROPOSIX_THREAD_H
#define MICROPOSIX_THREAD_H

#include <stdint.h>
#include <stddef.h>

typedef uint32_t mp_thread_id_t;

typedef enum {
    MP_THREAD_READY,
    MP_THREAD_RUNNING,
    MP_THREAD_BLOCKED,
    MP_THREAD_SLEEPING,
    MP_THREAD_TERMINATED
} mp_thread_state_t;

typedef struct mp_tcb {
    uintptr_t stack_ptr;        // Current stack pointer
    uintptr_t stack_base;       // Base address of the stack
    uint32_t  stack_size;       // Size of the stack in bytes
    mp_thread_id_t id;          // Thread ID
    char name[16];              // Thread name
    uint8_t priority;           // Thread priority
    mp_thread_state_t state;    // Current thread state
    uint32_t sleep_ticks;       // Ticks remaining in sleep state
    
    // TME (Thread Management Engine) fields
#if MICROPOSIX_PROFILE == 1
    uint64_t cpu_cycles;        // Accumulated CPU cycles (DWT-based)
    uint32_t context_switches;  // Number of context switches
    uintptr_t stack_hwm;        // Stack high-water-mark (last scanned)
    uint32_t heap_bytes;        // Current heap allocation in bytes
#endif
    
    struct mp_tcb *next;        // Next thread in list
} mp_tcb_t;

// Thread creation attributes
typedef struct {
    uint32_t stack_size;
    uint8_t priority;
    const char *name;
} mp_thread_attr_t;

// Function pointer for thread entry point
typedef void *(*mp_thread_func_t)(void *);

// Thread management functions
mp_thread_id_t mp_thread_create(mp_thread_func_t func, void *arg, const mp_thread_attr_t *attr);
void mp_thread_yield(void);
void mp_thread_exit(void *retval);
void mp_thread_sleep(uint32_t ms);
int mp_thread_join(mp_thread_id_t tid, void **retval);

// 64-bit monotonic clock
uint64_t mp_clock_get_monotonic_ms(void);
uint64_t mp_clock_get_monotonic_us(void);

#endif // MICROPOSIX_THREAD_H
