/**
 * @file core_sync.c
 * @brief Core Synchronization Implementation for nRF54L15
 * 
 * This file implements synchronization primitives for coordinating between
 * the Cortex-M33 and RISC-V cores on the nRF54L15 SoC.
 */

#include "microposix/hal/nrf54l15/shared/ipc.h"
#include "microposix/hal/nrf54l15/shared/shared_memory.h"
#include "microposix/hal/cpu.h"

// =============================================================================
// Core Barrier Implementation
// =============================================================================

/**
 * @brief Core Barrier Structure
 * 
 * Used to synchronize both cores at a specific point in execution.
 */
typedef struct {
    volatile uint32_t cores_arrived;  ///< Bitmask of cores that have arrived at the barrier
    volatile uint32_t cores_released; ///< Bitmask of cores that have been released
} core_barrier_t;

// Global barrier for core synchronization
static core_barrier_t core_barrier __attribute__((section(".shared_ram"))) = {0, 0};

/**
 * @brief Initialize the core barrier.
 */
void mp_hal_nrf54l15_core_barrier_init(void) {
    core_barrier.cores_arrived = 0;
    core_barrier.cores_released = 0;
}

/**
 * @brief Wait at the core barrier.
 * 
 * All cores that call this function will block until all cores have arrived.
 * 
 * @param timeout_cycles Timeout in CPU cycles (0 = no timeout).
 * @return true if synchronization was successful, false if timeout occurred.
 */
bool mp_hal_nrf54l15_core_barrier_wait(uint32_t timeout_cycles) {
    uint32_t core_id = mp_hal_nrf54l15_get_core_id();
    uint32_t start_cycles = mp_hal_get_ticks();
    
    // Arrive at the barrier
    uint32_t status = mp_hal_cpu_enter_critical();
    core_barrier.cores_arrived |= (1 << core_id);
    mp_hal_cpu_exit_critical(status);
    
    // Wait for all cores to arrive
    while (1) {
        status = mp_hal_cpu_enter_critical();
        
        if (core_barrier.cores_arrived == ((1 << CORE_ID_COUNT) - 1)) {
            // All cores have arrived, wait for release
            mp_hal_cpu_exit_critical(status);
            break;
        }
        
        mp_hal_cpu_exit_critical(status);
        
        // Check for timeout
        if (timeout_cycles > 0) {
            uint32_t elapsed = mp_hal_get_ticks() - start_cycles;
            if (elapsed >= timeout_cycles) {
                // Clear our arrival flag
                status = mp_hal_cpu_enter_critical();
                core_barrier.cores_arrived &= ~(1 << core_id);
                mp_hal_cpu_exit_critical(status);
                return false;
            }
        }
        
        // Yield to other threads
        mp_scheduler_yield();
    }
    
    // Wait for release
    while (1) {
        status = mp_hal_cpu_enter_critical();
        
        if (core_barrier.cores_released & (1 << core_id)) {
            mp_hal_cpu_exit_critical(status);
            return true;
        }
        
        mp_hal_cpu_exit_critical(status);
        mp_scheduler_yield();
    }
}

/**
 * @brief Release all cores waiting at the barrier.
 * 
 * This should be called after all cores have arrived and any shared
 * initialization is complete.
 */
void mp_hal_nrf54l15_core_barrier_release(void) {
    uint32_t status = mp_hal_cpu_enter_critical();
    core_barrier.cores_released = (1 << CORE_ID_COUNT) - 1;
    mp_hal_cpu_exit_critical(status);
}

// =============================================================================
// Core Rendezvous Implementation
// =============================================================================

/**
 * @brief Core Rendezvous Structure
 * 
 * Used for two cores to meet at a specific point and exchange data.
 */
typedef struct {
    volatile uint32_t core_ready[CORE_ID_COUNT];  ///< Ready flags for each core
    volatile uint32_t data[CORE_ID_COUNT];        ///< Data exchanged between cores
} core_rendezvous_t;

// Global rendezvous point
static core_rendezvous_t core_rendezvous __attribute__((section(".shared_ram"))) = {{0}, {0}};

/**
 * @brief Initialize the core rendezvous point.
 */
void mp_hal_nrf54l15_core_rendezvous_init(void) {
    for (uint32_t i = 0; i < CORE_ID_COUNT; i++) {
        core_rendezvous.core_ready[i] = 0;
        core_rendezvous.data[i] = 0;
    }
}

/**
 * @brief Meet at the rendezvous point with data.
 * 
 * @param data The data to exchange with the other core.
 * @param timeout_cycles Timeout in CPU cycles (0 = no timeout).
 * @return The data from the other core, or 0 if timeout occurred.
 */
uint32_t mp_hal_nrf54l15_core_rendezvous(uint32_t data, uint32_t timeout_cycles) {
    uint32_t core_id = mp_hal_nrf54l15_get_core_id();
    uint32_t other_core = mp_hal_nrf54l15_get_other_core_id();
    uint32_t start_cycles = mp_hal_get_ticks();
    
    // Set our data and mark as ready
    uint32_t status = mp_hal_cpu_enter_critical();
    core_rendezvous.data[core_id] = data;
    core_rendezvous.core_ready[core_id] = 1;
    mp_hal_cpu_exit_critical(status);
    
    // Wait for the other core to be ready
    while (1) {
        status = mp_hal_cpu_enter_critical();
        
        if (core_rendezvous.core_ready[other_core]) {
            uint32_t other_data = core_rendezvous.data[other_core];
            
            // Clear ready flags for next use
            core_rendezvous.core_ready[core_id] = 0;
            core_rendezvous.core_ready[other_core] = 0;
            
            mp_hal_cpu_exit_critical(status);
            return other_data;
        }
        
        mp_hal_cpu_exit_critical(status);
        
        // Check for timeout
        if (timeout_cycles > 0) {
            uint32_t elapsed = mp_hal_get_ticks() - start_cycles;
            if (elapsed >= timeout_cycles) {
                // Clear our ready flag
                status = mp_hal_cpu_enter_critical();
                core_rendezvous.core_ready[core_id] = 0;
                mp_hal_cpu_exit_critical(status);
                return 0;
            }
        }
        
        // Yield to other threads
        mp_scheduler_yield();
    }
}

// =============================================================================
// Core Handshake Implementation
// =============================================================================

/**
 * @brief Perform a handshake with the other core.
 * 
 * This is a simple protocol where each core signals its readiness and
 * waits for the other core to acknowledge.
 * 
 * @param timeout_cycles Timeout in CPU cycles (0 = no timeout).
 * @return true if handshake was successful, false if timeout occurred.
 */
bool mp_hal_nrf54l15_core_handshake(uint32_t timeout_cycles) {
    uint32_t core_id = mp_hal_nrf54l15_get_core_id();
    uint32_t other_core = mp_hal_nrf54l15_get_other_core_id();
    uint32_t start_cycles = mp_hal_get_ticks();
    
    // Signal our readiness
    mp_hal_nrf54l15_signal_core_ready();
    
    // Wait for the other core to be ready
    while (1) {
        if (mp_hal_nrf54l15_wait_for_other_core_ready(timeout_cycles)) {
            return true;
        }
        
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

// =============================================================================
// Core-Specific Initialization
// =============================================================================

/**
 * @brief Initialize core synchronization primitives.
 * 
 * This should be called early in the boot process for each core.
 */
void mp_hal_nrf54l15_core_sync_init(void) {
    mp_hal_nrf54l15_core_barrier_init();
    mp_hal_nrf54l15_core_rendezvous_init();
}

// =============================================================================
// Weak Implementations (to be overridden by core-specific code)
// =============================================================================

__attribute__((weak)) uint32_t mp_hal_get_ticks(void) {
    return 0;
}

__attribute__((weak)) void mp_scheduler_yield(void) {
    // Default implementation does nothing
}
