#ifndef MICROPOSIX_SHARED_HEAP_H
#define MICROPOSIX_SHARED_HEAP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @file shared_heap.h
 * @brief Shared heap memory management for multi-threaded environments
 * 
 * Provides thread-safe memory allocation with support for:
 * - Multiple heaps (per-thread, global, etc.)
 * - Thread-local storage integration
 * - Memory sharing between threads
 * - Alignment guarantees
 * - Allocation tracking and statistics
 */

// Forward declarations for thread types
#ifndef MP_THREAD_T_DEFINED
#define MP_THREAD_T_DEFINED
typedef struct mp_thread mp_thread_t;
#endif

#ifndef MP_MUTEX_T_DEFINED
#define MP_MUTEX_T_DEFINED
typedef struct mp_mutex mp_mutex_t;
#endif

// Forward declarations
typedef struct mp_shared_heap mp_shared_heap_t;

/**
 * @brief Shared heap configuration
 */
typedef struct {
    void *buffer;           ///< Memory buffer for the heap
    size_t size;           ///< Total size of the heap
    size_t min_block_size; ///< Minimum block size to allocate
    size_t alignment;      ///< Default alignment for allocations
    bool thread_safe;      ///< Whether to use thread-safe operations
    uint32_t max_threads;  ///< Maximum number of threads that can use this heap
} mp_shared_heap_config_t;

/**
 * @brief Shared heap statistics
 */
typedef struct {
    size_t total_size;      ///< Total size of the heap
    size_t used_size;       ///< Currently used bytes
    size_t free_size;       ///< Currently free bytes
    size_t peak_used;       ///< Peak used bytes
    size_t num_allocations; ///< Number of active allocations
    size_t num_frees;       ///< Number of free operations
    size_t num_reallocs;    ///< Number of reallocation operations
    size_t largest_free;    ///< Size of largest free block
    size_t fragmentation;   ///< Fragmentation percentage (0-100)
} mp_shared_heap_stats_t;

/**
 * @brief Allocation information for tracking
 */
typedef struct {
    void *ptr;             ///< Allocated pointer
    size_t size;           ///< Allocated size
    size_t alignment;      ///< Requested alignment
    mp_thread_t *thread;   ///< Thread that allocated this block
    const char *file;      ///< Source file of allocation
    int line;              ///< Source line of allocation
    uint32_t timestamp;    ///< Allocation timestamp (if available)
} mp_allocation_info_t;

/**
 * @brief Shared heap structure
 */
struct mp_shared_heap {
    void *buffer;              ///< Start of heap buffer
    size_t size;              ///< Total heap size
    size_t used;              ///< Currently used bytes
    size_t peak_used;          ///< Peak used bytes
    size_t alignment;         ///< Default alignment
    bool thread_safe;         ///< Thread-safe mode
    mp_mutex_t *mutex;        ///< Mutex for thread safety
    mp_shared_heap_stats_t stats; ///< Heap statistics
    
    // Allocation tracking
    mp_allocation_info_t *allocations; ///< Array of allocation info (if tracking enabled)
    size_t max_allocations;   ///< Maximum number of tracked allocations
    size_t num_allocations;   ///< Current number of tracked allocations
    
    // Thread-local caching (optional)
    void **thread_caches;     ///< Per-thread cache pointers
    
    // Implementation-specific data
    void *impl_data;          ///< Implementation-specific data (TLSF, buddy, etc.)
    
    // Free list for simple allocator
    struct block_header *free_list;
};

/**
 * @brief Block header for internal use
 */
typedef struct block_header {
    size_t size;              ///< Size of this block (including header)
    struct block_header *next; ///< Next block in free list
    struct block_header *prev; ///< Previous block in free list
    uint32_t magic;           ///< Magic number for validation
    bool is_free;            ///< Whether this block is free
    mp_thread_t *thread;     ///< Owning thread (for debugging)
    const char *file;        ///< Allocation source file
    int line;                ///< Allocation source line
} block_header_t;

#define MP_SHARED_HEAP_MAGIC 0x48454150  // "HEAP" in hex

/**
 * @brief Initialize a shared heap
 * 
 * @param heap The heap to initialize
 * @param config Configuration for the heap
 * @return 0 on success, -1 on error
 */
int mp_shared_heap_init(mp_shared_heap_t *heap, mp_shared_heap_config_t *config);

/**
 * @brief Destroy a shared heap
 * 
 * @param heap The heap to destroy
 */
void mp_shared_heap_destroy(mp_shared_heap_t *heap);

/**
 * @brief Allocate memory from a shared heap
 * 
 * @param heap The heap to allocate from
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
void *mp_shared_heap_alloc(mp_shared_heap_t *heap, size_t size);

/**
 * @brief Allocate aligned memory from a shared heap
 * 
 * @param heap The heap to allocate from
 * @param size Number of bytes to allocate
 * @param alignment Alignment requirement (must be power of 2)
 * @return Pointer to allocated memory, or NULL on failure
 */
void *mp_shared_heap_aligned_alloc(mp_shared_heap_t *heap, size_t size, size_t alignment);

/**
 * @brief Allocate and zero-initialize memory from a shared heap
 * 
 * @param heap The heap to allocate from
 * @param size Number of bytes to allocate
 * @return Pointer to allocated and zeroed memory, or NULL on failure
 */
void *mp_shared_heap_zalloc(mp_shared_heap_t *heap, size_t size);

/**
 * @brief Free memory allocated from a shared heap
 * 
 * @param heap The heap that allocated the memory
 * @param ptr Pointer to free
 */
void mp_shared_heap_free(mp_shared_heap_t *heap, void *ptr);

/**
 * @brief Reallocate memory from a shared heap
 * 
 * @param heap The heap that allocated the memory
 * @param ptr Pointer to reallocate, or NULL for new allocation
 * @param new_size New size in bytes
 * @return Pointer to reallocated memory, or NULL on failure
 */
void *mp_shared_heap_realloc(mp_shared_heap_t *heap, void *ptr, size_t new_size);

/**
 * @brief Allocate memory with tracking information
 * 
 * @param heap The heap to allocate from
 * @param size Number of bytes to allocate
 * @param file Source file name
 * @param line Source line number
 * @return Pointer to allocated memory, or NULL on failure
 */
void *mp_shared_heap_alloc_tracked(mp_shared_heap_t *heap, size_t size, const char *file, int line);

/**
 * @brief Free memory with tracking
 * 
 * @param heap The heap that allocated the memory
 * @param ptr Pointer to free
 * @param file Source file name
 * @param line Source line number
 */
void mp_shared_heap_free_tracked(mp_shared_heap_t *heap, void *ptr, const char *file, int line);

/**
 * @brief Get statistics for a shared heap
 * 
 * @param heap The heap to query
 * @param stats Output structure for statistics
 */
void mp_shared_heap_get_stats(mp_shared_heap_t *heap, mp_shared_heap_stats_t *stats);

/**
 * @brief Reset a shared heap (free all allocations)
 * 
 * @param heap The heap to reset
 */
void mp_shared_heap_reset(mp_shared_heap_t *heap);

/**
 * @brief Check if a pointer was allocated from a specific heap
 * 
 * @param heap The heap to check
 * @param ptr The pointer to verify
 * @return true if the pointer was allocated from this heap
 */
bool mp_shared_heap_contains(mp_shared_heap_t *heap, const void *ptr);

/**
 * @brief Get the size of an allocated block
 * 
 * @param heap The heap that allocated the block
 * @param ptr The pointer to query
 * @return Size of the allocated block, or 0 if not found
 */
size_t mp_shared_heap_get_block_size(mp_shared_heap_t *heap, const void *ptr);

/**
 * @brief Enable allocation tracking for a heap
 * 
 * @param heap The heap to configure
 * @param max_allocations Maximum number of allocations to track
 * @return 0 on success, -1 on error
 */
int mp_shared_heap_enable_tracking(mp_shared_heap_t *heap, size_t max_allocations);

/**
 * @brief Disable allocation tracking for a heap
 * 
 * @param heap The heap to configure
 */
void mp_shared_heap_disable_tracking(mp_shared_heap_t *heap);

/**
 * @brief Get allocation information by index
 * 
 * @param heap The heap to query
 * @param index Index of the allocation
 * @param info Output structure for allocation info
 * @return 0 on success, -1 on error
 */
int mp_shared_heap_get_allocation_info(mp_shared_heap_t *heap, size_t index, mp_allocation_info_t *info);

/**
 * @brief Dump heap information to a callback
 * 
 * @param heap The heap to dump
 * @param callback Function to receive each line of output
 * @param user_data User data to pass to callback
 */
typedef void (*mp_heap_dump_callback_t)(const char *line, void *user_data);
void mp_shared_heap_dump(mp_shared_heap_t *heap, mp_heap_dump_callback_t callback, void *user_data);

/**
 * @brief Validate heap integrity
 * 
 * @param heap The heap to validate
 * @return true if heap is valid
 */
bool mp_shared_heap_validate(mp_shared_heap_t *heap);

/**
 * @brief Defragment a shared heap
 * 
 * Attempts to coalesce free blocks to reduce fragmentation.
 * 
 * @param heap The heap to defragment
 * @return Number of blocks coalesced
 */
size_t mp_shared_heap_defrag(mp_shared_heap_t *heap);

/**
 * @brief Create a thread-local cache for a shared heap
 * 
 * @param heap The heap to add caching for
 * @param thread The thread to create cache for
 * @return 0 on success, -1 on error
 */
int mp_shared_heap_create_thread_cache(mp_shared_heap_t *heap, mp_thread_t *thread);

/**
 * @brief Destroy a thread-local cache
 * 
 * @param heap The heap
 * @param thread The thread whose cache to destroy
 */
void mp_shared_heap_destroy_thread_cache(mp_shared_heap_t *heap, mp_thread_t *thread);

/**
 * @brief Global shared heap instance
 */
extern mp_shared_heap_t *mp_shared_heap_global;

/**
 * @brief Initialize the global shared heap
 * 
 * @param buffer Memory buffer for the heap
 * @param size Size of the heap
 * @return 0 on success, -1 on error
 */
int mp_shared_heap_global_init(void *buffer, size_t size);

/**
 * @brief Allocate from the global shared heap
 * 
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, or NULL on failure
 */
void *mp_shared_heap_global_alloc(size_t size);

/**
 * @brief Free memory allocated from the global shared heap
 * 
 * @param ptr Pointer to free
 */
void mp_shared_heap_global_free(void *ptr);

/**
 * @brief Allocate from global heap with tracking
 * 
 * @param size Number of bytes
 * @param file Source file
 * @param line Source line
 * @return Pointer to allocated memory
 */
void *mp_shared_heap_global_alloc_tracked(size_t size, const char *file, int line);

/**
 * @brief Free from global heap with tracking
 * 
 * @param ptr Pointer to free
 * @param file Source file
 * @param line Source line
 */
void mp_shared_heap_global_free_tracked(void *ptr, const char *file, int line);

/**
 * @brief Get global heap statistics
 * 
 * @param stats Output structure
 */
void mp_shared_heap_global_get_stats(mp_shared_heap_stats_t *stats);

/**
 * @brief Check if pointer is in global heap
 * 
 * @param ptr Pointer to check
 * @return true if in global heap
 */
bool mp_shared_heap_global_contains(const void *ptr);

#endif // MICROPOSIX_SHARED_HEAP_H
