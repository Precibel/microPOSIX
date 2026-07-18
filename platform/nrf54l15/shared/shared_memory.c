/**
 * @file shared_memory.c
 * @brief Shared Memory Implementation for nRF54L15
 * 
 * This file implements the shared memory management for communication between
 * the Cortex-M33 and RISC-V cores on the nRF54L15 SoC.
 */

#include "microposix/hal/nrf54l15/shared/shared_memory.h"
#include "microposix/hal/cpu.h"
#include "microposix/kernel/scheduler.h"
#include <string.h>

// =============================================================================
// Shared Memory Initialization
// =============================================================================

void mp_hal_nrf54l15_shared_memory_init(void) {
    shared_memory_t *shared = mp_hal_nrf54l15_get_shared_memory();
    
    // Initialize thread registry
    shared->thread_registry.thread_count = 0;
    shared->thread_registry.next_global_id = 0;
    
    for (uint32_t i = 0; i < MAX_SHARED_THREADS; i++) {
        shared->thread_registry.threads[i].tcb = NULL;
        shared->thread_registry.threads[i].is_active = false;
    }
    
    // Initialize mailboxes
    shared->mailbox_m33_to_rv32.message = 0;
    shared->mailbox_m33_to_rv32.sender_core = CORE_ID_COUNT;
    shared->mailbox_m33_to_rv32.has_message = false;
    
    shared->mailbox_rv32_to_m33.message = 0;
    shared->mailbox_rv32_to_m33.sender_core = CORE_ID_COUNT;
    shared->mailbox_rv32_to_m33.has_message = false;
    
    // Initialize semaphores
    for (uint32_t i = 0; i < 4; i++) {
        shared->semaphores[i].count = 0;
        shared->semaphores[i].waiting_core = CORE_ID_COUNT;
    }
    
    // Initialize event flags
    shared->events.flags = 0;
    
    // Initialize global lock
    shared->global_lock.locked = 0;
    shared->global_lock.owner_core = CORE_ID_COUNT;
    
    // Initialize cross-core mutexes
    for (uint32_t i = 0; i < 8; i++) {
        shared->cross_core_mutexes[i].owner_core = CORE_ID_COUNT;
        shared->cross_core_mutexes[i].owner_thread = 0;
        shared->cross_core_mutexes[i].recursion_count = 0;
        shared->cross_core_mutexes[i].waiting_cores = 0;
        shared->cross_core_mutexes[i].lock.locked = 0;
        shared->cross_core_mutexes[i].lock.owner_core = CORE_ID_COUNT;
    }
    
    // Initialize system state
    shared->core_ready_flags = 0;
    shared->system_tick = 0;
    shared->last_activity[0] = 0;
    shared->last_activity[1] = 0;
}

// =============================================================================
// Thread Registry Operations
// =============================================================================

int32_t mp_hal_nrf54l15_register_shared_thread(mp_tcb_t *tcb, uint32_t core_id, const char *name) {
    shared_thread_registry_t *registry = mp_hal_nrf54l15_get_thread_registry();
    
    // Find a free slot
    for (uint32_t i = 0; i < MAX_SHARED_THREADS; i++) {
        if (!registry->threads[i].is_active) {
            uint32_t global_id = registry->next_global_id++;
            
            // Initialize the entry
            registry->threads[i].tcb = tcb;
            registry->threads[i].core_id = core_id;
            registry->threads[i].global_thread_id = global_id;
            registry->threads[i].local_thread_id = tcb->id;
            registry->threads[i].state = tcb->state;
            registry->threads[i].priority = tcb->priority;
            registry->threads[i].is_active = true;
            
            // Copy thread name
            strncpy(registry->threads[i].name, name, sizeof(registry->threads[i].name) - 1);
            registry->threads[i].name[sizeof(registry->threads[i].name) - 1] = '\0';
            
            // Increment thread count
            registry->thread_count++;
            
            return global_id;
        }
    }
    
    return -1; // No free slots
}

int32_t mp_hal_nrf54l15_unregister_shared_thread(uint32_t global_thread_id) {
    shared_thread_registry_t *registry = mp_hal_nrf54l15_get_thread_registry();
    
    for (uint32_t i = 0; i < MAX_SHARED_THREADS; i++) {
        if (registry->threads[i].global_thread_id == global_thread_id && 
            registry->threads[i].is_active) {
            
            // Mark as inactive
            registry->threads[i].is_active = false;
            registry->threads[i].tcb = NULL;
            
            // Decrement thread count
            registry->thread_count--;
            
            return 0;
        }
    }
    
    return -1; // Thread not found
}

shared_thread_entry_t *mp_hal_nrf54l15_lookup_shared_thread(uint32_t global_thread_id) {
    shared_thread_registry_t *registry = mp_hal_nrf54l15_get_thread_registry();
    
    for (uint32_t i = 0; i < MAX_SHARED_THREADS; i++) {
        if (registry->threads[i].global_thread_id == global_thread_id && 
            registry->threads[i].is_active) {
            return &registry->threads[i];
        }
    }
    
    return NULL; // Not found
}

// =============================================================================
// Spinlock Operations
// =============================================================================

bool mp_hal_nrf54l15_spinlock_acquire(shared_spinlock_t *lock, uint32_t timeout_cycles) {
    uint32_t core_id = mp_hal_nrf54l15_get_core_id();
    uint32_t start_cycles = mp_hal_get_ticks();
    
    while (1) {
        uint32_t status = mp_hal_cpu_enter_critical();
        
        if (!lock->locked) {
            lock->locked = 1;
            lock->owner_core = core_id;
            mp_hal_cpu_exit_critical(status);
            return true;
        }
        
        mp_hal_cpu_exit_critical(status);
        
        // Check for timeout
        if (timeout_cycles > 0) {
            uint32_t elapsed = mp_hal_get_ticks() - start_cycles;
            if (elapsed >= timeout_cycles) {
                return false;
            }
        }
        
        // Yield to other threads
        mp_scheduler_yield();
    }
}

void mp_hal_nrf54l15_spinlock_release(shared_spinlock_t *lock) {
    uint32_t core_id = mp_hal_nrf54l15_get_core_id();
    uint32_t status = mp_hal_cpu_enter_critical();
    
    // Only the owner can release the lock
    if (lock->owner_core == core_id && lock->locked) {
        lock->locked = 0;
        lock->owner_core = CORE_ID_COUNT;
    }
    
    mp_hal_cpu_exit_critical(status);
}

bool mp_hal_nrf54l15_spinlock_try_acquire(shared_spinlock_t *lock) {
    uint32_t core_id = mp_hal_nrf54l15_get_core_id();
    uint32_t status = mp_hal_cpu_enter_critical();
    
    if (!lock->locked) {
        lock->locked = 1;
        lock->owner_core = core_id;
        mp_hal_cpu_exit_critical(status);
        return true;
    }
    
    mp_hal_cpu_exit_critical(status);
    return false;
}

// =============================================================================
// Cross-Core Mutex Operations
// =============================================================================

bool mp_hal_nrf54l15_cross_core_mutex_lock(shared_mutex_t *mutex, uint32_t timeout_cycles) {
    uint32_t core_id = mp_hal_nrf54l15_get_core_id();
    uint32_t local_thread_id = mp_scheduler_get_current_thread_id();
    uint32_t start_cycles = mp_hal_get_ticks();
    
    // First, acquire the mutex's spinlock
    if (!mp_hal_nrf54l15_spinlock_acquire(&mutex->lock, timeout_cycles)) {
        return false; // Timeout acquiring spinlock
    }
    
    // Check if we already own the mutex (recursive lock)
    if (mutex->owner_core == core_id && mutex->owner_thread == local_thread_id) {
        mutex->recursion_count++;
        mp_hal_nrf54l15_spinlock_release(&mutex->lock);
        return true;
    }
    
    // Check if the mutex is available
    if (mutex->owner_core == CORE_ID_COUNT) {
        // Take the mutex
        mutex->owner_core = core_id;
        mutex->owner_thread = local_thread_id;
        mutex->recursion_count = 1;
        mp_hal_nrf54l15_spinlock_release(&mutex->lock);
        return true;
    }
    
    // Mutex is owned by someone else
    // Set our core as waiting
    mutex->waiting_cores |= (1 << core_id);
    
    // Release the spinlock and wait
    mp_hal_nrf54l15_spinlock_release(&mutex->lock);
    
    // Wait for the mutex to become available
    while (1) {
        // Try to acquire again
        if (!mp_hal_nrf54l15_spinlock_acquire(&mutex->lock, 100)) {
            // Check for timeout
            if (timeout_cycles > 0) {
                uint32_t elapsed = mp_hal_get_ticks() - start_cycles;
                if (elapsed >= timeout_cycles) {
                    // Clear our waiting flag
                    mutex->waiting_cores &= ~(1 << core_id);
                    mp_hal_nrf54l15_spinlock_release(&mutex->lock);
                    return false;
                }
            }
            continue;
        }
        
        // Check if we can take the mutex now
        if (mutex->owner_core == CORE_ID_COUNT) {
            mutex->owner_core = core_id;
            mutex->owner_thread = local_thread_id;
            mutex->recursion_count = 1;
            mutex->waiting_cores &= ~(1 << core_id);
            mp_hal_nrf54l15_spinlock_release(&mutex->lock);
            return true;
        }
        
        mp_hal_nrf54l15_spinlock_release(&mutex->lock);
        
        // Yield to other threads
        mp_scheduler_yield();
    }
}

void mp_hal_nrf54l15_cross_core_mutex_unlock(shared_mutex_t *mutex) {
    uint32_t core_id = mp_hal_nrf54l15_get_core_id();
    uint32_t local_thread_id = mp_scheduler_get_current_thread_id();
    
    // Acquire the spinlock
    mp_hal_nrf54l15_spinlock_acquire(&mutex->lock, 0);
    
    // Check if we own the mutex
    if (mutex->owner_core == core_id && mutex->owner_thread == local_thread_id) {
        if (mutex->recursion_count > 1) {
            mutex->recursion_count--;
        } else {
            // Release the mutex
            mutex->owner_core = CORE_ID_COUNT;
            mutex->owner_thread = 0;
            mutex->recursion_count = 0;
            
            // Wake up any waiting cores (implementation-specific)
            // This would typically trigger an IPC event
        }
    }
    
    mp_hal_nrf54l15_spinlock_release(&mutex->lock);
}

// =============================================================================
// Helper Functions (weak implementations)
// =============================================================================

// Weak implementation - cores should provide their own
__attribute__((weak)) uint32_t mp_hal_get_ticks(void) {
    return 0;
}

__attribute__((weak)) uint32_t mp_scheduler_get_current_thread_id(void) {
    return 0;
}
