#ifndef MICROPOSIX_MEMORY_H
#define MICROPOSIX_MEMORY_H

/**
 * @file memory.h
 * @brief Unified memory management header
 * 
 * This header provides a unified interface to all memory management features
 * in microPOSIX, including:
 * - Zero-copy memory views and slices
 * - Shared heap memory management
 * - Garbage collection
 * - Memory pools and arenas
 * - TLSF allocator
 */

#include "microposix/mm/pool.h"
#include "microposix/mm/tlsf.h"
#include "microposix/mm/memory_view.h"
#include "microposix/mm/shared_heap.h"
#include "microposix/mm/gc.h"

/**
 * @brief Memory management feature flags
 */
typedef enum {
    MP_MEMORY_FEATURE_POOL = (1 << 0),        ///< Fixed-size pool allocator
    MP_MEMORY_FEATURE_TLSF = (1 << 1),        ///< TLSF allocator
    MP_MEMORY_FEATURE_MEMORY_VIEW = (1 << 2), ///< Zero-copy memory views
    MP_MEMORY_FEATURE_SHARED_HEAP = (1 << 3), ///< Shared heap management
    MP_MEMORY_FEATURE_GC = (1 << 4),          ///< Garbage collection
    MP_MEMORY_FEATURE_ARENA = (1 << 5),       ///< Memory arena
} mp_memory_feature_t;

/**
 * @brief Memory statistics structure
 */
typedef struct {
    size_t total_allocated;      ///< Total bytes allocated
    size_t total_freed;          ///< Total bytes freed
    size_t current_usage;        ///< Current memory usage
    size_t peak_usage;           ///< Peak memory usage
    size_t num_allocations;      ///< Number of allocations
    size_t num_frees;            ///< Number of free operations
    size_t fragmentation;        ///< Fragmentation percentage
} mp_memory_stats_t;

/**
 * @brief Initialize memory subsystem
 * 
 * @param features Features to enable
 * @return 0 on success, -1 on error
 */
int mp_memory_init(uint32_t features);

/**
 * @brief Shutdown memory subsystem
 */
void mp_memory_shutdown(void);

/**
 * @brief Get global memory statistics
 * 
 * @param stats Output structure for statistics
 */
void mp_memory_get_stats(mp_memory_stats_t *stats);

/**
 * @brief Reset memory statistics
 */
void mp_memory_reset_stats(void);

/**
 * @brief Dump memory information
 * 
 * @param callback Callback to receive output lines
 * @param user_data User data for callback
 */
void mp_memory_dump(void (*callback)(const char *line, void *user_data), void *user_data);

/**
 * @brief Check memory integrity
 * 
 * @return true if memory subsystem is in a valid state
 */
bool mp_memory_validate(void);

/**
 * @brief Defragment memory
 * 
 * @return Number of blocks coalesced
 */
size_t mp_memory_defrag(void);

/**
 * @brief Set memory allocation failure handler
 * 
 * @param handler Function to call when allocation fails
 */
typedef void (*mp_memory_failure_handler_t)(void);
void mp_memory_set_failure_handler(mp_memory_failure_handler_t handler);

/**
 * @brief Get current memory usage
 * 
 * @return Current memory usage in bytes
 */
size_t mp_memory_get_usage(void);

/**
 * @brief Get peak memory usage
 * 
 * @return Peak memory usage in bytes
 */
size_t mp_memory_get_peak_usage(void);

/**
 * @brief Get available memory
 * 
 * @return Available memory in bytes
 */
size_t mp_memory_get_available(void);

/**
 * @brief Allocate memory with tracking
 * 
 * @param size Size to allocate
 * @param file Source file
 * @param line Source line
 * @return Pointer to allocated memory
 */
void *mp_memory_alloc_tracked(size_t size, const char *file, int line);

/**
 * @brief Free memory with tracking
 * 
 * @param ptr Pointer to free
 * @param file Source file
 * @param line Source line
 */
void mp_memory_free_tracked(void *ptr, const char *file, int line);

/**
 * @brief Macro for tracked allocation
 */
#define mp_memory_alloc(size) mp_memory_alloc_tracked(size, __FILE__, __LINE__)

/**
 * @brief Macro for tracked free
 */
#define mp_memory_free(ptr) mp_memory_free_tracked(ptr, __FILE__, __LINE__)

/**
 * @brief Allocate and zero memory
 * 
 * @param size Size to allocate
 * @return Pointer to allocated and zeroed memory
 */
void *mp_memory_zalloc(size_t size);

/**
 * @brief Reallocate memory
 * 
 * @param ptr Pointer to reallocate
 * @param new_size New size
 * @return Pointer to reallocated memory
 */
void *mp_memory_realloc(void *ptr, size_t new_size);

/**
 * @brief Allocate aligned memory
 * 
 * @param size Size to allocate
 * @param alignment Alignment requirement
 * @return Pointer to aligned memory
 */
void *mp_memory_aligned_alloc(size_t size, size_t alignment);

/**
 * @brief Free aligned memory
 * 
 * @param ptr Pointer to free
 */
void mp_memory_aligned_free(void *ptr);

/**
 * @brief Duplicate a string
 * 
 * @param str String to duplicate
 * @return New copy of the string
 */
char *mp_memory_strdup(const char *str);

/**
 * @brief Duplicate a string with length
 * 
 * @param str String to duplicate
 * @param length Length of string
 * @return New copy of the string
 */
char *mp_memory_strndup(const char *str, size_t length);

/**
 * @brief Compare two memory regions
 * 
 * @param a First region
 * @param b Second region
 * @param size Size to compare
 * @return 0 if equal, <0 if a < b, >0 if a > b
 */
int mp_memory_compare(const void *a, const void *b, size_t size);

/**
 * @brief Copy memory with overlap checking
 * 
 * @param dest Destination
 * @param src Source
 * @param size Size to copy
 * @return dest
 */
void *mp_memory_copy(void *dest, const void *src, size_t size);

/**
 * @brief Move memory
 * 
 * @param dest Destination
 * @param src Source
 * @param size Size to move
 * @return dest
 */
void *mp_memory_move(void *dest, const void *src, size_t size);

/**
 * @brief Set memory
 * 
 * @param dest Destination
 * @param value Value to set
 * @param size Size to set
 * @return dest
 */
void *mp_memory_set(void *dest, int value, size_t size);

/**
 * @brief Zero memory
 * 
 * @param dest Destination
 * @param size Size to zero
 * @return dest
 */
void *mp_memory_zero(void *dest, size_t size);

/**
 * @brief Find a byte in memory
 * 
 * @param mem Memory to search
 * @param byte Byte to find
 * @param size Size of memory
 * @return Pointer to first occurrence, or NULL if not found
 */
void *mp_memory_find(const void *mem, int byte, size_t size);

/**
 * @brief Find the last occurrence of a byte in memory
 * 
 * @param mem Memory to search
 * @param byte Byte to find
 * @param size Size of memory
 * @return Pointer to last occurrence, or NULL if not found
 */
void *mp_memory_rfind(const void *mem, int byte, size_t size);

/**
 * @brief Check if memory is all zeros
 * 
 * @param mem Memory to check
 * @param size Size of memory
 * @return true if all zeros
 */
bool mp_memory_is_zero(const void *mem, size_t size);

/**
 * @brief Swap two memory regions
 * 
 * @param a First region
 * @param b Second region
 * @param size Size of regions
 */
void mp_memory_swap(void *a, void *b, size_t size);

/**
 * @brief Reverse memory
 * 
 * @param mem Memory to reverse
 * @param size Size of memory
 */
void mp_memory_reverse(void *mem, size_t size);

/**
 * @brief Calculate CRC32 checksum
 * 
 * @param mem Memory to checksum
 * @param size Size of memory
 * @return CRC32 checksum
 */
uint32_t mp_memory_crc32(const void *mem, size_t size);

/**
 * @brief Calculate simple checksum
 * 
 * @param mem Memory to checksum
 * @param size Size of memory
 * @return Simple checksum
 */
uint8_t mp_memory_checksum(const void *mem, size_t size);

#endif // MICROPOSIX_MEMORY_H
