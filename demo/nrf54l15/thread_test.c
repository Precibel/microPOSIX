/**
 * @file thread_test.c
 * @brief Thread Test for nRF54L15 Demo
 * 
 * This file contains tests for the threading functionality on nRF54L15.
 */

#include <stdint.h>
#include "microposix/kernel/thread.h"
#include "microposix/kernel/scheduler.h"
#include "microposix/hal/nrf54l15/shared/shared_memory.h"

// =============================================================================
// Thread Test Functions
// =============================================================================

/**
 * @brief Test thread creation and management.
 */
void thread_creation_test(void) {
    mp_tcb_t tcb1, tcb2;
    uint32_t stack1[128];
    uint32_t stack2[128];
    
    // Create thread 1
    if (mp_thread_create(&tcb1, thread_test_func_1, NULL, 5, "Test_Thread_1") == 0) {
        // Thread created successfully
    }
    
    // Create thread 2
    if (mp_thread_create(&tcb2, thread_test_func_2, NULL, 5, "Test_Thread_2") == 0) {
        // Thread created successfully
    }
    
    // Register threads in shared registry
    mp_hal_nrf54l15_register_shared_thread(&tcb1, mp_hal_nrf54l15_get_core_id(), "Test_Thread_1");
    mp_hal_nrf54l15_register_shared_thread(&tcb2, mp_hal_nrf54l15_get_core_id(), "Test_Thread_2");
    
    // Lookup threads
    shared_thread_entry_t *entry1 = mp_hal_nrf54l15_lookup_shared_thread(0);
    shared_thread_entry_t *entry2 = mp_hal_nrf54l15_lookup_shared_thread(1);
    
    (void)entry1;
    (void)entry2;
    
    // Unregister threads
    mp_hal_nrf54l15_unregister_shared_thread(0);
    mp_hal_nrf54l15_unregister_shared_thread(1);
}

/**
 * @brief Test thread suspension and resumption.
 */
void thread_suspend_resume_test(void) {
    mp_tcb_t tcb;
    uint32_t stack[128];
    
    // Create a thread
    if (mp_thread_create(&tcb, thread_test_func_3, NULL, 5, "Suspend_Test_Thread") == 0) {
        // Suspend the thread
        mp_thread_suspend(&tcb);
        
        // Resume the thread
        mp_thread_resume(&tcb);
        
        // Suspend again
        mp_thread_suspend(&tcb);
        
        // Resume again
        mp_thread_resume(&tcb);
    }
}

/**
 * @brief Test thread priority.
 */
void thread_priority_test(void) {
    mp_tcb_t tcb_high, tcb_low;
    uint32_t stack_high[128];
    uint32_t stack_low[128];
    
    // Create high priority thread
    if (mp_thread_create(&tcb_high, thread_test_func_high, NULL, 10, "High_Priority") == 0) {
        // Success
    }
    
    // Create low priority thread
    if (mp_thread_create(&tcb_low, thread_test_func_low, NULL, 1, "Low_Priority") == 0) {
        // Success
    }
    
    // The high priority thread should run first
}

// =============================================================================
// Test Thread Functions
// =============================================================================

static volatile uint32_t thread_test_counter_1 = 0;
static volatile uint32_t thread_test_counter_2 = 0;
static volatile uint32_t thread_test_counter_3 = 0;
static volatile uint32_t thread_test_counter_high = 0;
static volatile uint32_t thread_test_counter_low = 0;

void thread_test_func_1(void *arg) {
    (void)arg;
    while (1) {
        thread_test_counter_1++;
        mp_scheduler_yield();
    }
}

void thread_test_func_2(void *arg) {
    (void)arg;
    while (1) {
        thread_test_counter_2++;
        mp_scheduler_yield();
    }
}

void thread_test_func_3(void *arg) {
    (void)arg;
    while (1) {
        thread_test_counter_3++;
        mp_scheduler_yield();
    }
}

void thread_test_func_high(void *arg) {
    (void)arg;
    while (1) {
        thread_test_counter_high++;
        mp_scheduler_yield();
    }
}

void thread_test_func_low(void *arg) {
    (void)arg;
    while (1) {
        thread_test_counter_low++;
        mp_scheduler_yield();
    }
}

// =============================================================================
// Conditional Compilation
// =============================================================================

#if DEMO_THREAD_TEST
/**
 * @brief Thread test entry point (called from main).
 */
void demo_thread_test(void) {
    thread_creation_test();
    thread_suspend_resume_test();
    thread_priority_test();
}
#endif
