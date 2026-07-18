/**
 * @file main.c
 * @brief Main Demo Application for nRF54L15
 * 
 * This is the main entry point for the nRF54L15 demo application.
 * It demonstrates dual-core functionality with IPC, threading, and synchronization.
 */

#include <stdint.h>
#include <stdbool.h>
#include "microposix/kernel/thread.h"
#include "microposix/kernel/scheduler.h"
#include "microposix/hal/nrf54l15/shared/ipc.h"
#include "microposix/hal/nrf54l15/shared/shared_memory.h"
#include "microposix/hal/nrf54l15/shared/core_sync.h"
#include "microposix/hal/cpu.h"

// =============================================================================
// Demo Configuration
// =============================================================================

// Enable/disable specific demo features
#ifndef DEMO_IPC_TEST
#define DEMO_IPC_TEST 1
#endif

#ifndef DEMO_THREAD_TEST
#define DEMO_THREAD_TEST 1
#endif

#ifndef DEMO_SYNC_TEST
#define DEMO_SYNC_TEST 1
#endif

// =============================================================================
// Function Prototypes
// =============================================================================

// Demo test functions
void demo_ipc_test(void);
void demo_thread_test(void);
void demo_sync_test(void);

// Thread functions
void m33_thread_1(void *arg);
void m33_thread_2(void *arg);
void rv32_thread_1(void *arg);
void rv32_thread_2(void *arg);

// =============================================================================
// Global Variables
// =============================================================================

// Thread TCBs
static mp_tcb_t m33_thread_1_tcb;
static mp_tcb_t m33_thread_2_tcb;
static mp_tcb_t rv32_thread_1_tcb;
static mp_tcb_t rv32_thread_2_tcb;

// Thread stacks
static uint32_t m33_thread_1_stack[256];
static uint32_t m33_thread_2_stack[256];
static uint32_t rv32_thread_1_stack[256];
static uint32_t rv32_thread_2_stack[256];

// Test counters
static volatile uint32_t m33_counter = 0;
static volatile uint32_t rv32_counter = 0;
static volatile uint32_t ipc_message_count = 0;

// =============================================================================
// Main Function
// =============================================================================

int main(void) {
    uint32_t core_id = mp_hal_nrf54l15_get_core_id();
    
    // Initialize the shared memory (only once, by M33)
    if (core_id == CORE_ID_M33) {
        mp_hal_nrf54l15_shared_memory_init();
    }
    
    // Initialize core synchronization
    mp_hal_nrf54l15_core_sync_init();
    
    // Signal that this core is ready
    mp_hal_nrf54l15_signal_core_ready();
    
    // Wait for the other core to be ready
    if (!mp_hal_nrf54l15_wait_for_other_core_ready(1000)) {
        // Timeout - continue anyway (other core might not be running)
    }
    
    // Initialize the scheduler
    mp_scheduler_init();
    
    // Print core information (via UART if available)
    // For now, we'll just use a simple delay to simulate work
    
    // Create threads based on the core
    if (core_id == CORE_ID_M33) {
        // Create M33 threads
        mp_thread_create(&m33_thread_1_tcb, m33_thread_1, NULL, 5, "M33_Thread_1");
        mp_thread_create(&m33_thread_2_tcb, m33_thread_2, NULL, 5, "M33_Thread_2");
    } else {
        // Create RV32 threads
        mp_thread_create(&rv32_thread_1_tcb, rv32_thread_1, NULL, 5, "RV32_Thread_1");
        mp_thread_create(&rv32_thread_2_tcb, rv32_thread_2, NULL, 5, "RV32_Thread_2");
    }
    
    // Run the demo tests
    #if DEMO_IPC_TEST
    demo_ipc_test();
    #endif
    
    #if DEMO_THREAD_TEST
    demo_thread_test();
    #endif
    
    #if DEMO_SYNC_TEST
    demo_sync_test();
    #endif
    
    // Start the scheduler
    mp_scheduler_start();
    
    // Should never reach here
    while (1) {
        mp_hal_cpu_delay(1000);
    }
    
    return 0;
}

// =============================================================================
// Thread Functions
// =============================================================================

void m33_thread_1(void *arg) {
    (void)arg;
    uint32_t core_id = mp_hal_nrf54l15_get_core_id();
    
    while (1) {
        m33_counter++;
        
        // Send a message to RV32 every 100 iterations
        if ((m33_counter % 100) == 0) {
            ipc_message_t msg;
            mp_hal_nrf54l15_ipc_init_message(&msg, IPC_MSG_TYPE_CUSTOM);
            msg.param1 = 0xM33THREAD1;
            msg.param2 = m33_counter;
            
            if (mp_hal_nrf54l15_ipc_send_nonblocking(&msg) == 0) {
                ipc_message_count++;
            }
        }
        
        // Delay to simulate work
        mp_hal_cpu_delay(100);
    }
}

void m33_thread_2(void *arg) {
    (void)arg;
    
    while (1) {
        m33_counter += 2;
        
        // Send a message to RV32 every 150 iterations
        if ((m33_counter % 150) == 0) {
            ipc_message_t msg;
            mp_hal_nrf54l15_ipc_init_message(&msg, IPC_MSG_TYPE_CUSTOM);
            msg.param1 = 0xM33THREAD2;
            msg.param2 = m33_counter;
            
            if (mp_hal_nrf54l15_ipc_send_nonblocking(&msg) == 0) {
                ipc_message_count++;
            }
        }
        
        // Delay to simulate work
        mp_hal_cpu_delay(150);
    }
}

void rv32_thread_1(void *arg) {
    (void)arg;
    
    while (1) {
        rv32_counter++;
        
        // Check for messages from M33
        ipc_message_t msg;
        if (mp_hal_nrf54l15_ipc_receive_nonblocking(&msg) == 0) {
            // Process the message
            if (msg.param1 == 0xM33THREAD1 || msg.param1 == 0xM33THREAD2) {
                // Echo the message back
                msg.sender_core = CORE_ID_RV32;
                msg.receiver_core = CORE_ID_M33;
                msg.checksum = mp_hal_nrf54l15_ipc_calculate_checksum(&msg);
                mp_hal_nrf54l15_ipc_send_nonblocking(&msg);
            }
        }
        
        // Delay to simulate work
        mp_hal_cpu_delay(100);
    }
}

void rv32_thread_2(void *arg) {
    (void)arg;
    
    while (1) {
        rv32_counter += 2;
        
        // Check for messages from M33
        ipc_message_t msg;
        if (mp_hal_nrf54l15_ipc_receive_nonblocking(&msg) == 0) {
            // Process the message
            if (msg.param1 == 0xM33THREAD1 || msg.param1 == 0xM33THREAD2) {
                // Just count the messages
                ipc_message_count++;
            }
        }
        
        // Delay to simulate work
        mp_hal_cpu_delay(200);
    }
}

// =============================================================================
// Demo Test Functions
// =============================================================================

void demo_ipc_test(void) {
    uint32_t core_id = mp_hal_nrf54l15_get_core_id();
    
    // Send a test message to the other core
    ipc_message_t msg;
    mp_hal_nrf54l15_ipc_init_message(&msg, IPC_MSG_TYPE_CUSTOM);
    msg.param1 = 0xDEADBEEF;
    msg.param2 = core_id;
    msg.param3 = 0xCAFEBABE;
    
    if (mp_hal_nrf54l15_ipc_send_blocking(&msg) == 0) {
        // Message sent successfully
    }
    
    // Try to receive a response
    if (mp_hal_nrf54l15_ipc_receive_nonblocking(&msg) == 0) {
        // Message received
    }
}

void demo_thread_test(void) {
    uint32_t core_id = mp_hal_nrf54l15_get_core_id();
    
    // Register threads in the shared registry
    if (core_id == CORE_ID_M33) {
        mp_hal_nrf54l15_register_shared_thread(&m33_thread_1_tcb, CORE_ID_M33, "M33_Thread_1");
        mp_hal_nrf54l15_register_shared_thread(&m33_thread_2_tcb, CORE_ID_M33, "M33_Thread_2");
    } else {
        mp_hal_nrf54l15_register_shared_thread(&rv32_thread_1_tcb, CORE_ID_RV32, "RV32_Thread_1");
        mp_hal_nrf54l15_register_shared_thread(&rv32_thread_2_tcb, CORE_ID_RV32, "RV32_Thread_2");
    }
}

void demo_sync_test(void) {
    uint32_t core_id = mp_hal_nrf54l15_get_core_id();
    
    // Test the barrier synchronization
    mp_hal_nrf54l15_core_barrier_wait(1000000);
    
    // All cores have reached this point
    
    // Test the rendezvous
    uint32_t data = core_id;
    uint32_t other_data = mp_hal_nrf54l15_core_rendezvous(data, 1000000);
    
    // other_data should be the core ID of the other core
    (void)other_data;
    
    // Test the handshake
    mp_hal_nrf54l15_core_handshake(1000000);
}
