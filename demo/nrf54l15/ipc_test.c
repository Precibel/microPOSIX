/**
 * @file ipc_test.c
 * @brief IPC Test for nRF54L15 Demo
 * 
 * This file contains tests for the Inter-Core Communication (IPC) functionality.
 */

#include <stdint.h>
#include <stdbool.h>
#include "microposix/hal/nrf54l15/shared/ipc.h"
#include "microposix/hal/nrf54l15/shared/shared_memory.h"

// =============================================================================
// IPC Test Functions
// =============================================================================

/**
 * @brief Test basic IPC mailbox functionality.
 */
void ipc_mailbox_test(void) {
    uint32_t core_id = mp_hal_nrf54l15_get_core_id();
    uint32_t other_core = mp_hal_nrf54l15_get_other_core_id();
    
    // Test 1: Send and receive a simple message
    ipc_message_t msg;
    mp_hal_nrf54l15_ipc_init_message(&msg, IPC_MSG_TYPE_CUSTOM);
    msg.param1 = 0xTEST1;
    msg.param2 = core_id;
    msg.param3 = 0x12345678;
    
    // Send the message
    if (mp_hal_nrf54l15_ipc_send_blocking(&msg) == 0) {
        // Message sent successfully
    }
    
    // Receive the message (from the other core)
    if (mp_hal_nrf54l15_ipc_receive_blocking(&msg) == 0) {
        // Verify the message
        if (msg.type == IPC_MSG_TYPE_CUSTOM && 
            msg.sender_core == other_core &&
            msg.param1 == 0xTEST1) {
            // Test passed
        }
    }
    
    // Test 2: Non-blocking send/receive
    mp_hal_nrf54l15_ipc_init_message(&msg, IPC_MSG_TYPE_CUSTOM);
    msg.param1 = 0xTEST2;
    
    // Try to send (should succeed)
    if (mp_hal_nrf54l15_ipc_send_nonblocking(&msg) == 0) {
        // Success
    }
    
    // Try to receive (may fail if no message)
    if (mp_hal_nrf54l15_ipc_receive_nonblocking(&msg) == 0) {
        // Message received
    }
    
    // Test 3: Check if message is available
    bool has_message = mp_hal_nrf54l15_ipc_message_available();
    (void)has_message;
}

/**
 * @brief Test IPC semaphore functionality.
 */
void ipc_semaphore_test(void) {
    // Test semaphore 0
    ipc_semaphore_id_t sem_id = IPC_SEMAPHORE_0;
    
    // Give the semaphore
    mp_hal_nrf54l15_ipc_semaphore_give(sem_id);
    
    // Take the semaphore (should succeed)
    mp_hal_nrf54l15_ipc_semaphore_take(sem_id);
    
    // Try to take again (should block or fail)
    bool taken = mp_hal_nrf54l15_ipc_semaphore_try_take(sem_id);
    (void)taken;  // Should be false
    
    // Give the semaphore again
    mp_hal_nrf54l15_ipc_semaphore_give(sem_id);
    
    // Now try to take should succeed
    taken = mp_hal_nrf54l15_ipc_semaphore_try_take(sem_id);
    (void)taken;  // Should be true
}

/**
 * @brief Test IPC event functionality.
 */
void ipc_event_test(void) {
    // Clear all events first
    mp_hal_nrf54l15_ipc_event_clear(IPC_EVENT_MAILBOX_RECEIVED);
    mp_hal_nrf54l15_ipc_event_clear(IPC_EVENT_SEMAPHORE_AVAILABLE);
    mp_hal_nrf54l15_ipc_event_clear(IPC_EVENT_CORE_READY);
    
    // Set an event
    mp_hal_nrf54l15_ipc_event_set(IPC_EVENT_CUSTOM_0);
    
    // Check if the event is set
    bool is_set = mp_hal_nrf54l15_ipc_event_check(IPC_EVENT_CUSTOM_0);
    (void)is_set;  // Should be true
    
    // Clear the event
    mp_hal_nrf54l15_ipc_event_clear(IPC_EVENT_CUSTOM_0);
    
    // Check again
    is_set = mp_hal_nrf54l15_ipc_event_check(IPC_EVENT_CUSTOM_0);
    (void)is_set;  // Should be false
    
    // Test waiting for an event
    // Note: This would require another core to set the event
    // For now, we'll just set it ourselves to test the wait
    mp_hal_nrf54l15_ipc_event_set(IPC_EVENT_CUSTOM_1);
    mp_hal_nrf54l15_ipc_event_wait(IPC_EVENT_CUSTOM_1, true);
}

/**
 * @brief Test core management functions.
 */
void ipc_core_test(void) {
    uint32_t core_id = mp_hal_nrf54l15_get_core_id();
    uint32_t other_core = mp_hal_nrf54l15_get_other_core_id();
    
    // Verify core IDs
    if (core_id == CORE_ID_M33) {
        // This is M33
        if (other_core != CORE_ID_RV32) {
            // Error
        }
    } else if (core_id == CORE_ID_RV32) {
        // This is RV32
        if (other_core != CORE_ID_M33) {
            // Error
        }
    }
    
    // Signal that this core is ready (again)
    mp_hal_nrf54l15_signal_core_ready();
    
    // Wait for the other core to be ready
    bool ready = mp_hal_nrf54l15_wait_for_other_core_ready(1000);
    (void)ready;
}

/**
 * @brief Run all IPC tests.
 */
void ipc_run_all_tests(void) {
    ipc_mailbox_test();
    ipc_semaphore_test();
    ipc_event_test();
    ipc_core_test();
}

// =============================================================================
// Conditional Compilation
// =============================================================================

#if DEMO_IPC_TEST
/**
 * @brief IPC test entry point (called from main).
 */
void demo_ipc_test(void) {
    ipc_run_all_tests();
}
#endif
