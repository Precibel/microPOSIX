/**
 * @file sync_test.c
 * @brief Synchronization Test for nRF54L15 Demo
 * 
 * This file contains tests for the synchronization primitives on nRF54L15.
 */

#include <stdint.h>
#include "microposix/hal/nrf54l15/shared/shared_memory.h"
#include "microposix/hal/nrf54l15/shared/core_sync.h"

// =============================================================================
// Synchronization Test Functions
// =============================================================================

/**
 * @brief Test spinlock functionality.
 */
void spinlock_test(void) {
    shared_spinlock_t *lock = mp_hal_nrf54l15_get_global_lock();
    
    // Test 1: Acquire and release
    if (mp_hal_nrf54l15_spinlock_try_acquire(lock)) {
        // Lock acquired
        mp_hal_nrf54l15_spinlock_release(lock);
    }
    
    // Test 2: Try acquire when locked
    mp_hal_nrf54l15_spinlock_acquire(lock, 0);
    bool acquired = mp_hal_nrf54l15_spinlock_try_acquire(lock);
    (void)acquired;  // Should be false
    mp_hal_nrf54l15_spinlock_release(lock);
    
    // Test 3: Acquire with timeout
    mp_hal_nrf54l15_spinlock_acquire(lock, 1000);
    mp_hal_nrf54l15_spinlock_release(lock);
}

/**
 * @brief Test cross-core mutex functionality.
 */
void cross_core_mutex_test(void) {
    shared_mutex_t *mutex = mp_hal_nrf54l15_get_cross_core_mutex(0);
    
    // Test 1: Lock and unlock
    if (mp_hal_nrf54l15_cross_core_mutex_lock(mutex, 1000)) {
        // Mutex locked
        mp_hal_nrf54l15_cross_core_mutex_unlock(mutex);
    }
    
    // Test 2: Try to lock when already locked
    mp_hal_nrf54l15_cross_core_mutex_lock(mutex, 1000);
    bool locked = mp_hal_nrf54l15_cross_core_mutex_lock(mutex, 100);
    (void)locked;  // Should be false (timeout)
    mp_hal_nrf54l15_cross_core_mutex_unlock(mutex);
    
    // Test 3: Recursive lock
    mp_hal_nrf54l15_cross_core_mutex_lock(mutex, 1000);
    locked = mp_hal_nrf54l15_cross_core_mutex_lock(mutex, 1000);
    (void)locked;  // Should be true (recursive)
    mp_hal_nrf54l15_cross_core_mutex_unlock(mutex);
    mp_hal_nrf54l15_cross_core_mutex_unlock(mutex);
}

/**
 * @brief Test core barrier functionality.
 */
void barrier_test(void) {
    // Reset the barrier
    mp_hal_nrf54l15_core_barrier_init();
    
    // Wait at the barrier
    bool success = mp_hal_nrf54l15_core_barrier_wait(1000000);
    (void)success;
    
    // Release the barrier (only one core should do this)
    if (mp_hal_nrf54l15_get_core_id() == CORE_ID_M33) {
        mp_hal_nrf54l15_core_barrier_release();
    }
}

/**
 * @brief Test core rendezvous functionality.
 */
void rendezvous_test(void) {
    uint32_t data = mp_hal_nrf54l15_get_core_id();
    uint32_t other_data = mp_hal_nrf54l15_core_rendezvous(data, 1000000);
    
    // other_data should be the core ID of the other core
    (void)other_data;
}

/**
 * @brief Test core handshake functionality.
 */
void handshake_test(void) {
    bool success = mp_hal_nrf54l15_core_handshake(1000000);
    (void)success;
}

/**
 * @brief Test shared memory access.
 */
void shared_memory_test(void) {
    shared_memory_t *shared = mp_hal_nrf54l15_get_shared_memory();
    
    // Test 1: Access system tick
    uint32_t tick = shared->system_tick;
    (void)tick;
    
    // Test 2: Access core ready flags
    uint32_t ready_flags = shared->core_ready_flags;
    (void)ready_flags;
    
    // Test 3: Access thread registry
    shared_thread_registry_t *registry = mp_hal_nrf54l15_get_thread_registry();
    uint32_t thread_count = registry->thread_count;
    (void)thread_count;
}

// =============================================================================
// Conditional Compilation
// =============================================================================

#if DEMO_SYNC_TEST
/**
 * @brief Synchronization test entry point (called from main).
 */
void demo_sync_test(void) {
    spinlock_test();
    cross_core_mutex_test();
    barrier_test();
    rendezvous_test();
    handshake_test();
    shared_memory_test();
}
#endif
