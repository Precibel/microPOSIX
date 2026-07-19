/**
 * @file ipc.c
 * @brief Inter-Core Communication (IPC) Implementation for nRF54L15
 * 
 * This file implements the IPC layer for communication between the Cortex-M33
 * and RISC-V cores on the nRF54L15 SoC.
 */

#include "microposix/hal/nrf54l15/shared/ipc.h"
#include "microposix/hal/cpu.h"
#include "microposix/kernel/scheduler.h"

// =============================================================================
// Local Definitions
// =============================================================================

// Timeout for blocking operations (in CPU cycles)
#define IPC_BLOCKING_TIMEOUT_CYCLES  1000000

// =============================================================================
// Local Variables
// =============================================================================

// Track core readiness
static volatile bool core_ready[CORE_ID_COUNT] = {false};

// =============================================================================
// Initialization
// =============================================================================

void mp_hal_nrf54l15_ipc_init(void) {
    // Enable IPC interrupts (core-specific implementation)
    mp_hal_nrf54l15_ipc_enable_interrupts();
    
    // Mark this core as not ready yet
    core_ready[mp_hal_nrf54l15_get_core_id()] = false;
}

// =============================================================================
// Core-Specific Interrupt Enable (implemented per core)
// =============================================================================

// Weak function - each core must implement this
__attribute__((weak)) void mp_hal_nrf54l15_ipc_enable_interrupts(void) {
    // Default implementation does nothing
    // Each core (M33/RV32) should override this
}

// =============================================================================
// Mailbox Operations
// =============================================================================

int mp_hal_nrf54l15_ipc_send_blocking(const ipc_message_t *msg) {
    uint32_t timeout = IPC_BLOCKING_TIMEOUT_CYCLES;
    
    while (timeout-- > 0) {
        uint32_t status = mp_hal_cpu_enter_critical();
        
        // Check if mailbox is empty (sender register is 0)
        if (NRF54L15_IPC_MAILBOX_SENDER == 0) {
            // Write the message
            NRF54L15_IPC_MAILBOX_SENDER = *(const uint32_t *)msg;
            mp_hal_cpu_exit_critical(status);
            
            // Trigger an event to notify the other core
            mp_hal_nrf54l15_ipc_event_set(IPC_EVENT_MAILBOX_RECEIVED);
            
            return 0; // Success
        }
        
        mp_hal_cpu_exit_critical(status);
        
        // Small delay to prevent busy-waiting
        mp_hal_cpu_delay(10);
    }
    
    return -1; // Timeout
}

int mp_hal_nrf54l15_ipc_receive_blocking(ipc_message_t *msg) {
    uint32_t timeout = IPC_BLOCKING_TIMEOUT_CYCLES;
    
    while (timeout-- > 0) {
        uint32_t status = mp_hal_cpu_enter_critical();
        
        // Check if mailbox has data (receiver register is non-zero)
        if (NRF54L15_IPC_MAILBOX_RECEIVER != 0) {
            // Read the message
            *(uint32_t *)msg = NRF54L15_IPC_MAILBOX_RECEIVER;
            
            // Validate checksum
            if (!mp_hal_nrf54l15_ipc_validate_checksum(msg)) {
                mp_hal_cpu_exit_critical(status);
                return -1; // Invalid checksum
            }
            
            // Clear the mailbox
            NRF54L15_IPC_MAILBOX_RECEIVER = 0;
            
            mp_hal_cpu_exit_critical(status);
            return 0; // Success
        }
        
        mp_hal_cpu_exit_critical(status);
        
        // Small delay to prevent busy-waiting
        mp_hal_cpu_delay(10);
    }
    
    return -1; // Timeout
}

int mp_hal_nrf54l15_ipc_send_nonblocking(const ipc_message_t *msg) {
    uint32_t status = mp_hal_cpu_enter_critical();
    
    if (NRF54L15_IPC_MAILBOX_SENDER == 0) {
        NRF54L15_IPC_MAILBOX_SENDER = *(const uint32_t *)msg;
        mp_hal_cpu_exit_critical(status);
        
        // Trigger an event to notify the other core
        mp_hal_nrf54l15_ipc_event_set(IPC_EVENT_MAILBOX_RECEIVED);
        
        return 0; // Success
    }
    
    mp_hal_cpu_exit_critical(status);
    return -1; // Mailbox full
}

int mp_hal_nrf54l15_ipc_receive_nonblocking(ipc_message_t *msg) {
    uint32_t status = mp_hal_cpu_enter_critical();
    
    if (NRF54L15_IPC_MAILBOX_RECEIVER != 0) {
        *(uint32_t *)msg = NRF54L15_IPC_MAILBOX_RECEIVER;
        
        if (!mp_hal_nrf54l15_ipc_validate_checksum(msg)) {
            mp_hal_cpu_exit_critical(status);
            return -1; // Invalid checksum
        }
        
        NRF54L15_IPC_MAILBOX_RECEIVER = 0;
        mp_hal_cpu_exit_critical(status);
        return 0; // Success
    }
    
    mp_hal_cpu_exit_critical(status);
    return -1; // No message available
}

bool mp_hal_nrf54l15_ipc_message_available(void) {
    uint32_t status = mp_hal_cpu_enter_critical();
    bool available = (NRF54L15_IPC_MAILBOX_RECEIVER != 0);
    mp_hal_cpu_exit_critical(status);
    return available;
}

// =============================================================================
// Semaphore Operations
// =============================================================================

void mp_hal_nrf54l15_ipc_semaphore_give(ipc_semaphore_id_t sem_id) {
    volatile uint32_t *sem_reg = NULL;
    
    switch (sem_id) {
        case IPC_SEMAPHORE_0: sem_reg = &NRF54L15_IPC_SEMAPHORE_0; break;
        case IPC_SEMAPHORE_1: sem_reg = &NRF54L15_IPC_SEMAPHORE_1; break;
        case IPC_SEMAPHORE_2: sem_reg = &NRF54L15_IPC_SEMAPHORE_2; break;
        case IPC_SEMAPHORE_3: sem_reg = &NRF54L15_IPC_SEMAPHORE_3; break;
        default: return;
    }
    
    uint32_t status = mp_hal_cpu_enter_critical();
    *sem_reg = 1; // Set semaphore
    mp_hal_cpu_exit_critical(status);
    
    // Trigger event
    mp_hal_nrf54l15_ipc_event_set(IPC_EVENT_SEMAPHORE_AVAILABLE);
}

void mp_hal_nrf54l15_ipc_semaphore_take(ipc_semaphore_id_t sem_id) {
    volatile uint32_t *sem_reg = NULL;
    
    switch (sem_id) {
        case IPC_SEMAPHORE_0: sem_reg = &NRF54L15_IPC_SEMAPHORE_0; break;
        case IPC_SEMAPHORE_1: sem_reg = &NRF54L15_IPC_SEMAPHORE_1; break;
        case IPC_SEMAPHORE_2: sem_reg = &NRF54L15_IPC_SEMAPHORE_2; break;
        case IPC_SEMAPHORE_3: sem_reg = &NRF54L15_IPC_SEMAPHORE_3; break;
        default: return;
    }
    
    uint32_t timeout = IPC_BLOCKING_TIMEOUT_CYCLES;
    
    while (timeout-- > 0) {
        uint32_t status = mp_hal_cpu_enter_critical();
        
        if (*sem_reg != 0) {
            *sem_reg = 0; // Clear semaphore
            mp_hal_cpu_exit_critical(status);
            return;
        }
        
        mp_hal_cpu_exit_critical(status);
        mp_hal_cpu_delay(10);
    }
}

bool mp_hal_nrf54l15_ipc_semaphore_try_take(ipc_semaphore_id_t sem_id) {
    volatile uint32_t *sem_reg = NULL;
    
    switch (sem_id) {
        case IPC_SEMAPHORE_0: sem_reg = &NRF54L15_IPC_SEMAPHORE_0; break;
        case IPC_SEMAPHORE_1: sem_reg = &NRF54L15_IPC_SEMAPHORE_1; break;
        case IPC_SEMAPHORE_2: sem_reg = &NRF54L15_IPC_SEMAPHORE_2; break;
        case IPC_SEMAPHORE_3: sem_reg = &NRF54L15_IPC_SEMAPHORE_3; break;
        default: return false;
    }
    
    uint32_t status = mp_hal_cpu_enter_critical();
    
    if (*sem_reg != 0) {
        *sem_reg = 0;
        mp_hal_cpu_exit_critical(status);
        return true;
    }
    
    mp_hal_cpu_exit_critical(status);
    return false;
}

// =============================================================================
// Event Operations
// =============================================================================

void mp_hal_nrf54l15_ipc_event_set(ipc_event_flags_t event) {
    uint32_t status = mp_hal_cpu_enter_critical();
    NRF54L15_IPC_EVENT_SEND |= event;
    mp_hal_cpu_exit_critical(status);
}

void mp_hal_nrf54l15_ipc_event_clear(ipc_event_flags_t event) {
    uint32_t status = mp_hal_cpu_enter_critical();
    NRF54L15_IPC_EVENT_SEND &= ~event;
    mp_hal_cpu_exit_critical(status);
}

void mp_hal_nrf54l15_ipc_event_wait(ipc_event_flags_t events, bool clear_on_exit) {
    uint32_t timeout = IPC_BLOCKING_TIMEOUT_CYCLES;
    
    while (timeout-- > 0) {
        uint32_t status = mp_hal_cpu_enter_critical();
        
        if ((NRF54L15_IPC_EVENT_RECEIVE & events) == events) {
            if (clear_on_exit) {
                NRF54L15_IPC_EVENT_RECEIVE &= ~events;
            }
            mp_hal_cpu_exit_critical(status);
            return;
        }
        
        mp_hal_cpu_exit_critical(status);
        mp_hal_cpu_delay(10);
    }
}

bool mp_hal_nrf54l15_ipc_event_check(ipc_event_flags_t event) {
    uint32_t status = mp_hal_cpu_enter_critical();
    bool set = (NRF54L15_IPC_EVENT_RECEIVE & event) != 0;
    mp_hal_cpu_exit_critical(status);
    return set;
}

// =============================================================================
// Core Management
// =============================================================================

void mp_hal_nrf54l15_start_other_core(void) {
    // Set the core start register to start the other core
    NRF54L15_CORESTART = 1;
}

uint32_t mp_hal_nrf54l15_get_core_id(void) {
    // This should be overridden by each core's implementation
    // Default to M33 for safety
    return CORE_ID_M33;
}

uint32_t mp_hal_nrf54l15_get_other_core_id(void) {
    return (mp_hal_nrf54l15_get_core_id() == CORE_ID_M33) ? CORE_ID_RV32 : CORE_ID_M33;
}

void mp_hal_nrf54l15_signal_core_ready(void) {
    uint32_t core_id = mp_hal_nrf54l15_get_core_id();
    core_ready[core_id] = true;
    
    // Send a message to the other core
    ipc_message_t msg;
    mp_hal_nrf54l15_ipc_init_message(&msg, IPC_MSG_TYPE_CORE_READY);
    mp_hal_nrf54l15_ipc_send_nonblocking(&msg);
    
    // Set the event
    mp_hal_nrf54l15_ipc_event_set(IPC_EVENT_CORE_READY);
}

bool mp_hal_nrf54l15_wait_for_other_core_ready(uint32_t timeout_ms) {
    uint32_t other_core = mp_hal_nrf54l15_get_other_core_id();
    uint32_t start_time = mp_hal_get_ticks();
    
    while (1) {
        if (core_ready[other_core]) {
            return true;
        }
        
        // Check for timeout
        if (timeout_ms > 0) {
            uint32_t elapsed = mp_hal_get_ticks() - start_time;
            if (elapsed >= timeout_ms) {
                return false;
            }
        }
        
        // Yield to other threads
        mp_scheduler_yield();
    }
}

// =============================================================================
// Helper Functions (weak implementations - cores should override)
// =============================================================================

// Weak implementation - cores should provide their own
__attribute__((weak)) uint32_t mp_hal_get_ticks(void) {
    return 0;
}

__attribute__((weak)) void mp_hal_cpu_delay(uint32_t cycles) {
    volatile uint32_t i;
    for (i = 0; i < cycles; i++) {
        __asm volatile ("nop");
    }
}
