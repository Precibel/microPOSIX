/**
 * @file ipc.h
 * @brief Inter-Core Communication (IPC) for nRF54L15
 * 
 * This header provides the interface for communication between the Cortex-M33
 * and RISC-V cores on the nRF54L15 SoC. It includes mailbox, semaphore, and
 * event-based synchronization primitives.
 */

#ifndef MICROPOSIX_NRF54L15_IPC_H
#define MICROPOSIX_NRF54L15_IPC_H

#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// IPC Hardware Registers (nRF54L15-specific)
// =============================================================================

// IPC Base Address (from nRF54L15 datasheet)
#define NRF54L15_IPC_BASE               0x50000000

// Mailbox Registers (32-bit)
#define NRF54L15_IPC_MAILBOX_SENDER     (*((volatile uint32_t *)(NRF54L15_IPC_BASE + 0x000)))
#define NRF54L15_IPC_MAILBOX_RECEIVER   (*((volatile uint32_t *)(NRF54L15_IPC_BASE + 0x004)))

// Semaphore Registers
#define NRF54L15_IPC_SEMAPHORE_0        (*((volatile uint32_t *)(NRF54L15_IPC_BASE + 0x010)))
#define NRF54L15_IPC_SEMAPHORE_1        (*((volatile uint32_t *)(NRF54L15_IPC_BASE + 0x014)))
#define NRF54L15_IPC_SEMAPHORE_2        (*((volatile uint32_t *)(NRF54L15_IPC_BASE + 0x018)))
#define NRF54L15_IPC_SEMAPHORE_3        (*((volatile uint32_t *)(NRF54L15_IPC_BASE + 0x01C)))

// Event Registers
#define NRF54L15_IPC_EVENT_SEND         (*((volatile uint32_t *)(NRF54L15_IPC_BASE + 0x020)))
#define NRF54L15_IPC_EVENT_RECEIVE      (*((volatile uint32_t *)(NRF54L15_IPC_BASE + 0x024)))

// Interrupt Enable/Disable Registers
#define NRF54L15_IPC_INTENSET           (*((volatile uint32_t *)(NRF54L15_IPC_BASE + 0x030)))
#define NRF54L15_IPC_INTENCLR           (*((volatile uint32_t *)(NRF54L15_IPC_BASE + 0x034)))

// Interrupt Status Registers
#define NRF54L15_IPC_INTSTATUS          (*((volatile uint32_t *)(NRF54L15_IPC_BASE + 0x040)))

// Core Start Register (to start the other core)
#define NRF54L15_CORESTART             (*((volatile uint32_t *)(NRF54L15_IPC_BASE + 0x100)))

// =============================================================================
// IPC Message Types
// =============================================================================

/**
 * @brief IPC Message Types
 * 
 * These are used to identify the type of message being sent between cores.
 */
typedef enum {
    IPC_MSG_TYPE_INVALID = 0,
    
    // Core lifecycle messages
    IPC_MSG_TYPE_CORE_READY = 1,        ///< Sent when a core finishes initialization
    IPC_MSG_TYPE_CORE_HALT = 2,         ///< Sent when a core is halting
    
    // Thread management messages
    IPC_MSG_TYPE_THREAD_CREATE = 10,   ///< Request to create a thread on the other core
    IPC_MSG_TYPE_THREAD_TERMINATE = 11, ///< Request to terminate a thread on the other core
    IPC_MSG_TYPE_THREAD_SUSPEND = 12,   ///< Request to suspend a thread
    IPC_MSG_TYPE_THREAD_RESUME = 13,    ///< Request to resume a thread
    
    // Synchronization messages
    IPC_MSG_TYPE_MUTEX_LOCK = 20,      ///< Request to lock a cross-core mutex
    IPC_MSG_TYPE_MUTEX_UNLOCK = 21,    ///< Request to unlock a cross-core mutex
    IPC_MSG_TYPE_SEMAPHORE_GIVE = 22,  ///< Semaphore give operation
    IPC_MSG_TYPE_SEMAPHORE_TAKE = 23,  ///< Semaphore take operation
    
    // Data transfer messages
    IPC_MSG_TYPE_DATA_TRANSFER = 30,  ///< Generic data transfer
    IPC_MSG_TYPE_DATA_ACK = 31,        ///< Acknowledgment for data transfer
    
    // Custom application messages (start from 100)
    IPC_MSG_TYPE_CUSTOM = 100
} ipc_msg_type_t;

/**
 * @brief IPC Message Structure
 * 
 * Standard format for messages sent between cores.
 */
typedef struct __attribute__((packed)) {
    ipc_msg_type_t type;       ///< Message type
    uint32_t sender_core;      ///< Sending core ID (0 = M33, 1 = RV32)
    uint32_t receiver_core;    ///< Receiving core ID
    uint32_t param1;           ///< Parameter 1 (meaning depends on message type)
    uint32_t param2;           ///< Parameter 2
    uint32_t param3;           ///< Parameter 3
    uint32_t checksum;         ///< Simple checksum for validation
} ipc_message_t;

// =============================================================================
// IPC Semaphore Types
// =============================================================================

/**
 * @brief IPC Semaphore IDs
 */
typedef enum {
    IPC_SEMAPHORE_0 = 0,
    IPC_SEMAPHORE_1 = 1,
    IPC_SEMAPHORE_2 = 2,
    IPC_SEMAPHORE_3 = 3,
    IPC_SEMAPHORE_COUNT
} ipc_semaphore_id_t;

// =============================================================================
// IPC Event Types
// =============================================================================

/**
 * @brief IPC Event Flags
 */
typedef enum {
    IPC_EVENT_MAILBOX_RECEIVED = (1 << 0),  ///< New message in mailbox
    IPC_EVENT_SEMAPHORE_AVAILABLE = (1 << 1), ///< Semaphore is available
    IPC_EVENT_CORE_READY = (1 << 2),        ///< Other core is ready
    IPC_EVENT_CUSTOM_0 = (1 << 8),          ///< Custom event 0
    IPC_EVENT_CUSTOM_1 = (1 << 9),          ///< Custom event 1
} ipc_event_flags_t;

// =============================================================================
// Core IDs
// =============================================================================

#define CORE_ID_M33    0  ///< Cortex-M33 core
#define CORE_ID_RV32   1  ///< RISC-V core
#define CORE_ID_COUNT  2  ///< Total number of cores

// =============================================================================
// Function Prototypes
// =============================================================================

// --- Initialization ---

/**
 * @brief Initialize the IPC system for the current core.
 * 
 * This function must be called early in the boot process for each core.
 * It sets up the IPC hardware and enables interrupts.
 */
void mp_hal_nrf54l15_ipc_init(void);

// --- Mailbox Operations ---

/**
 * @brief Send a message to the other core (blocking).
 * 
 * @param msg The message to send.
 * @return 0 on success, -1 on error.
 */
int mp_hal_nrf54l15_ipc_send_blocking(const ipc_message_t *msg);

/**
 * @brief Receive a message from the other core (blocking).
 * 
 * @param msg Buffer to store the received message.
 * @return 0 on success, -1 on error.
 */
int mp_hal_nrf54l15_ipc_receive_blocking(ipc_message_t *msg);

/**
 * @brief Send a message to the other core (non-blocking).
 * 
 * @param msg The message to send.
 * @return 0 on success, -1 if mailbox is full.
 */
int mp_hal_nrf54l15_ipc_send_nonblocking(const ipc_message_t *msg);

/**
 * @brief Receive a message from the other core (non-blocking).
 * 
 * @param msg Buffer to store the received message.
 * @return 0 on success, -1 if no message is available.
 */
int mp_hal_nrf54l15_ipc_receive_nonblocking(ipc_message_t *msg);

/**
 * @brief Check if a message is available in the mailbox.
 * 
 * @return true if a message is available, false otherwise.
 */
bool mp_hal_nrf54l15_ipc_message_available(void);

// --- Semaphore Operations ---

/**
 * @brief Give (release) a semaphore.
 * 
 * @param sem_id The semaphore ID.
 */
void mp_hal_nrf54l15_ipc_semaphore_give(ipc_semaphore_id_t sem_id);

/**
 * @brief Take (acquire) a semaphore (blocking).
 * 
 * @param sem_id The semaphore ID.
 */
void mp_hal_nrf54l15_ipc_semaphore_take(ipc_semaphore_id_t sem_id);

/**
 * @brief Try to take a semaphore (non-blocking).
 * 
 * @param sem_id The semaphore ID.
 * @return true if the semaphore was acquired, false otherwise.
 */
bool mp_hal_nrf54l15_ipc_semaphore_try_take(ipc_semaphore_id_t sem_id);

// --- Event Operations ---

/**
 * @brief Set an event flag.
 * 
 * @param event The event flag to set.
 */
void mp_hal_nrf54l15_ipc_event_set(ipc_event_flags_t event);

/**
 * @brief Clear an event flag.
 * 
 * @param event The event flag to clear.
 */
void mp_hal_nrf54l15_ipc_event_clear(ipc_event_flags_t event);

/**
 * @brief Wait for one or more event flags (blocking).
 * 
 * @param events The event flags to wait for.
 * @param clear_on_exit If true, clear the events before returning.
 */
void mp_hal_nrf54l15_ipc_event_wait(ipc_event_flags_t events, bool clear_on_exit);

/**
 * @brief Check if an event flag is set.
 * 
 * @param event The event flag to check.
 * @return true if the event is set, false otherwise.
 */
bool mp_hal_nrf54l15_ipc_event_check(ipc_event_flags_t event);

// --- Core Management ---

/**
 * @brief Start the other core.
 * 
 * This function is typically called by the M33 core to start the RISC-V core.
 */
void mp_hal_nrf54l15_start_other_core(void);

/**
 * @brief Get the current core ID.
 * 
 * @return CORE_ID_M33 or CORE_ID_RV32.
 */
uint32_t mp_hal_nrf54l15_get_core_id(void);

/**
 * @brief Get the ID of the other core.
 * 
 * @return CORE_ID_RV32 if current core is M33, CORE_ID_M33 if current core is RV32.
 */
uint32_t mp_hal_nrf54l15_get_other_core_id(void);

/**
 * @brief Signal that the current core is ready.
 * 
 * This should be called after the core has completed initialization.
 */
void mp_hal_nrf54l15_signal_core_ready(void);

/**
 * @brief Wait for the other core to be ready.
 * 
 * @param timeout_ms Timeout in milliseconds (0 = wait forever).
 * @return true if the other core is ready, false if timeout occurred.
 */
bool mp_hal_nrf54l15_wait_for_other_core_ready(uint32_t timeout_ms);

// --- Utility Functions ---

/**
 * @brief Calculate a simple checksum for an IPC message.
 * 
 * @param msg The message to calculate checksum for.
 * @return The checksum value.
 */
static inline uint32_t mp_hal_nrf54l15_ipc_calculate_checksum(const ipc_message_t *msg) {
    uint32_t sum = msg->type + msg->sender_core + msg->receiver_core + 
                  msg->param1 + msg->param2 + msg->param3;
    return sum ^ (sum >> 16);
}

/**
 * @brief Validate an IPC message checksum.
 * 
 * @param msg The message to validate.
 * @return true if the checksum is valid, false otherwise.
 */
static inline bool mp_hal_nrf54l15_ipc_validate_checksum(const ipc_message_t *msg) {
    return msg->checksum == mp_hal_nrf54l15_ipc_calculate_checksum(msg);
}

/**
 * @brief Initialize an IPC message with default values.
 * 
 * @param msg The message to initialize.
 * @param type The message type.
 */
static inline void mp_hal_nrf54l15_ipc_init_message(ipc_message_t *msg, ipc_msg_type_t type) {
    msg->type = type;
    msg->sender_core = mp_hal_nrf54l15_get_core_id();
    msg->receiver_core = mp_hal_nrf54l15_get_other_core_id();
    msg->param1 = 0;
    msg->param2 = 0;
    msg->param3 = 0;
    msg->checksum = mp_hal_nrf54l15_ipc_calculate_checksum(msg);
}

#endif // MICROPOSIX_NRF54L15_IPC_H
