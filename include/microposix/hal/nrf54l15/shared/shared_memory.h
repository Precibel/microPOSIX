/**
 * @file shared_memory.h
 * @brief Shared Memory Layout for nRF54L15
 * 
 * This header defines the layout of shared memory between the Cortex-M33
 * and RISC-V cores on the nRF54L15 SoC. It includes structures for thread
 * management, IPC, and other shared data.
 */

#ifndef MICROPOSIX_NRF54L15_SHARED_MEMORY_H
#define MICROPOSIX_NRF54L15_SHARED_MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include "microposix/kernel/thread.h"

// =============================================================================
// Shared Memory Configuration
// =============================================================================

// Shared SRAM base address (from nRF54L15 datasheet)
// Note: Actual address may vary - check the specific nRF54L15 memory map
#define NRF54L15_SHARED_SRAM_BASE      0x20000000

// Shared SRAM size (64KB as per nRF54L15 datasheet)
#define NRF54L15_SHARED_SRAM_SIZE      (64 * 1024)

// =============================================================================
// Shared Memory Layout
// =============================================================================

// --- Thread Management ---

/**
 * @brief Maximum number of threads that can be shared between cores.
 */
#define MAX_SHARED_THREADS            64

/**
 * @brief Shared Thread Registry Entry
 * 
 * Each entry represents a thread that is visible to both cores.
 */
typedef struct {
    mp_tcb_t *tcb;              ///< Pointer to the Thread Control Block (core-local)
    uint32_t core_id;           ///< Core ID where the thread is running (0 = M33, 1 = RV32)
    uint32_t global_thread_id;  ///< Global thread ID (unique across both cores)
    uint32_t local_thread_id;   ///< Local thread ID (core-specific)
    char name[16];              ///< Thread name (null-terminated)
    uint32_t state;             ///< Thread state (from mp_thread_state_t)
    uint32_t priority;          ///< Thread priority
    bool is_active;             ///< Whether the thread is currently active
} shared_thread_entry_t;

/**
 * @brief Shared Thread Registry
 */
typedef struct {
    shared_thread_entry_t threads[MAX_SHARED_THREADS];
    uint32_t thread_count;                          ///< Number of registered threads
    uint32_t next_global_id;                        ///< Next available global thread ID
} shared_thread_registry_t;

// --- IPC ---

/**
 * @brief IPC Mailbox
 * 
 * Used for message passing between cores.
 */
typedef struct {
    volatile uint32_t message;       ///< The message data
    volatile uint32_t sender_core;   ///< Core ID of the sender
    volatile bool has_message;       ///< Flag indicating if a message is available
} shared_mailbox_t;

/**
 * @brief IPC Semaphore
 */
typedef struct {
    volatile uint32_t count;         ///< Semaphore count
    volatile uint32_t waiting_core;  ///< Core ID waiting on this semaphore (if any)
} shared_semaphore_t;

/**
 * @brief IPC Event Flags
 */
typedef struct {
    volatile uint32_t flags;         ///< Event flags
} shared_event_flags_t;

// --- Synchronization ---

/**
 * @brief Spinlock for protecting shared data
 */
typedef struct {
    volatile uint32_t locked;        ///< 0 = unlocked, 1 = locked
    volatile uint32_t owner_core;    ///< Core ID that holds the lock
} shared_spinlock_t;

// --- Cross-Core Mutex ---

/**
 * @brief Cross-Core Mutex
 * 
 * Extended mutex that can be locked by threads on either core.
 */
typedef struct {
    volatile uint32_t owner_core;    ///< Core ID that owns the mutex (CORE_ID_COUNT = none)
    volatile uint32_t owner_thread;  ///< Local thread ID of the owner
    volatile uint32_t recursion_count; ///< Recursion count for the owner
    volatile uint32_t waiting_cores;  ///< Bitmask of cores waiting for this mutex
    shared_spinlock_t lock;          ///< Spinlock for atomic operations
} shared_mutex_t;

// --- Memory Management ---

/**
 * @brief Shared Memory Pool
 * 
 * Used for allocating memory that can be accessed by both cores.
 */
typedef struct {
    uint8_t data[1];  ///< Flexible array member for pool data
} shared_memory_pool_t;

// =============================================================================
// Main Shared Memory Structure
// =============================================================================

/**
 * @brief Main Shared Memory Structure
 * 
 * This structure defines the complete layout of shared memory between cores.
 * All fields must be properly aligned for both Arm and RISC-V access.
 */
typedef struct __attribute__((packed, aligned(8))) {
    // --- Thread Management ---
    shared_thread_registry_t thread_registry;
    
    // --- IPC ---
    shared_mailbox_t mailbox_m33_to_rv32;
    shared_mailbox_t mailbox_rv32_to_m33;
    shared_semaphore_t semaphores[4];
    shared_event_flags_t events;
    
    // --- Synchronization ---
    shared_spinlock_t global_lock;           ///< Global spinlock for critical sections
    shared_mutex_t cross_core_mutexes[8];   ///< Cross-core mutexes
    
    // --- System State ---
    volatile uint32_t core_ready_flags;      ///< Bitmask of ready cores (bit 0 = M33, bit 1 = RV32)
    volatile uint32_t system_tick;           ///< Global system tick count
    volatile uint32_t last_activity[2];      ///< Last activity timestamp per core
    
    // --- Memory Management ---
    // Reserved space for shared memory pool
    uint8_t memory_pool[NRF54L15_SHARED_SRAM_SIZE - 
                        sizeof(shared_thread_registry_t) - 
                        2 * sizeof(shared_mailbox_t) - 
                        4 * sizeof(shared_semaphore_t) - 
                        sizeof(shared_event_flags_t) - 
                        sizeof(shared_spinlock_t) - 
                        8 * sizeof(shared_mutex_t) - 
                        3 * sizeof(uint32_t)];
} shared_memory_t;

// =============================================================================
// Shared Memory Access
// =============================================================================

/**
 * @brief Get a pointer to the shared memory region.
 * 
 * @return Pointer to the shared memory structure.
 */
static inline shared_memory_t *mp_hal_nrf54l15_get_shared_memory(void) {
    return (shared_memory_t *)NRF54L15_SHARED_SRAM_BASE;
}

/**
 * @brief Get a pointer to the thread registry.
 * 
 * @return Pointer to the shared thread registry.
 */
static inline shared_thread_registry_t *mp_hal_nrf54l15_get_thread_registry(void) {
    return &mp_hal_nrf54l15_get_shared_memory()->thread_registry;
}

/**
 * @brief Get a pointer to the mailbox for sending to the other core.
 * 
 * @param from_core The core ID sending the message.
 * @return Pointer to the appropriate mailbox.
 */
static inline shared_mailbox_t *mp_hal_nrf54l15_get_send_mailbox(uint32_t from_core) {
    shared_memory_t *shared = mp_hal_nrf54l15_get_shared_memory();
    return (from_core == CORE_ID_M33) ? &shared->mailbox_m33_to_rv32 : &shared->mailbox_rv32_to_m33;
}

/**
 * @brief Get a pointer to the mailbox for receiving from the other core.
 * 
 * @param to_core The core ID receiving the message.
 * @return Pointer to the appropriate mailbox.
 */
static inline shared_mailbox_t *mp_hal_nrf54l15_get_receive_mailbox(uint32_t to_core) {
    shared_memory_t *shared = mp_hal_nrf54l15_get_shared_memory();
    return (to_core == CORE_ID_M33) ? &shared->mailbox_rv32_to_m33 : &shared->mailbox_m33_to_rv32;
}

/**
 * @brief Get a pointer to a shared semaphore.
 * 
 * @param sem_id The semaphore ID (0-3).
 * @return Pointer to the semaphore.
 */
static inline shared_semaphore_t *mp_hal_nrf54l15_get_semaphore(uint32_t sem_id) {
    if (sem_id >= 4) return NULL;
    return &mp_hal_nrf54l15_get_shared_memory()->semaphores[sem_id];
}

/**
 * @brief Get a pointer to a cross-core mutex.
 * 
 * @param mutex_id The mutex ID (0-7).
 * @return Pointer to the mutex.
 */
static inline shared_mutex_t *mp_hal_nrf54l15_get_cross_core_mutex(uint32_t mutex_id) {
    if (mutex_id >= 8) return NULL;
    return &mp_hal_nrf54l15_get_shared_memory()->cross_core_mutexes[mutex_id];
}

/**
 * @brief Get the global spinlock.
 * 
 * @return Pointer to the global spinlock.
 */
static inline shared_spinlock_t *mp_hal_nrf54l15_get_global_lock(void) {
    return &mp_hal_nrf54l15_get_shared_memory()->global_lock;
}

// =============================================================================
// Shared Memory Initialization
// =============================================================================

/**
 * @brief Initialize the shared memory region.
 * 
 * This function should be called by the first core to boot (typically M33).
 */
void mp_hal_nrf54l15_shared_memory_init(void);

/**
 * @brief Register a thread in the shared registry.
 * 
 * @param tcb Pointer to the thread's TCB.
 * @param core_id The core ID where the thread is running.
 * @param name The thread name.
 * @return The global thread ID, or -1 on error.
 */
int32_t mp_hal_nrf54l15_register_shared_thread(mp_tcb_t *tcb, uint32_t core_id, const char *name);

/**
 * @brief Unregister a thread from the shared registry.
 * 
 * @param global_thread_id The global thread ID to unregister.
 * @return 0 on success, -1 on error.
 */
int32_t mp_hal_nrf54l15_unregister_shared_thread(uint32_t global_thread_id);

/**
 * @brief Lookup a thread by its global ID.
 * 
 * @param global_thread_id The global thread ID.
 * @return Pointer to the shared thread entry, or NULL if not found.
 */
shared_thread_entry_t *mp_hal_nrf54l15_lookup_shared_thread(uint32_t global_thread_id);

// =============================================================================
// Spinlock Operations
// =============================================================================

/**
 * @brief Acquire a spinlock.
 * 
 * @param lock Pointer to the spinlock.
 * @param timeout_cycles Timeout in CPU cycles (0 = no timeout).
 * @return true if the lock was acquired, false if timeout occurred.
 */
bool mp_hal_nrf54l15_spinlock_acquire(shared_spinlock_t *lock, uint32_t timeout_cycles);

/**
 * @brief Release a spinlock.
 * 
 * @param lock Pointer to the spinlock.
 */
void mp_hal_nrf54l15_spinlock_release(shared_spinlock_t *lock);

/**
 * @brief Try to acquire a spinlock (non-blocking).
 * 
 * @param lock Pointer to the spinlock.
 * @return true if the lock was acquired, false otherwise.
 */
bool mp_hal_nrf54l15_spinlock_try_acquire(shared_spinlock_t *lock);

#endif // MICROPOSIX_NRF54L15_SHARED_MEMORY_H
