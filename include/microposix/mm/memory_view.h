#ifndef MICROPOSIX_MEMORY_VIEW_H
#define MICROPOSIX_MEMORY_VIEW_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @file memory_view.h
 * @brief Zero-copy memory view and slice utilities
 * 
 * Provides zero-copy memory views for efficient data sharing without copying.
 * Memory views allow multiple components to access the same underlying buffer
 * with different interpretations (types, strides, etc.)
 */

/**
 * @brief A memory view that provides zero-copy access to a buffer
 * 
 * Memory views allow efficient slicing and reinterpretation of memory
 * without actual data copying. Useful for protocol parsing, buffer management,
 * and zero-copy data processing.
 */
typedef struct {
    uint8_t *data;          ///< Pointer to the start of the view
    size_t length;          ///< Length of the view in bytes
    size_t capacity;        ///< Total capacity of the underlying buffer
    size_t offset;          ///< Offset from the start of the underlying buffer
    bool owns_data;         ///< Whether this view owns the underlying data
    void (*free_fn)(void*); ///< Function to free data if owns_data is true
} mp_memory_view_t;

/**
 * @brief A typed memory slice for zero-copy access
 * 
 * Provides type-safe access to a memory region with element count.
 */
typedef struct {
    void *data;             ///< Pointer to the start of the slice
    size_t element_size;    ///< Size of each element in bytes
    size_t element_count;   ///< Number of elements in the slice
    size_t capacity;        ///< Total capacity in elements
    bool owns_data;         ///< Whether this slice owns the underlying data
    void (*free_fn)(void*); ///< Function to free data if owns_data is true
} mp_memory_slice_t;

/**
 * @brief A memory arena for bulk allocation and zero-copy slicing
 * 
 * Arenas allow allocation of multiple objects from a single buffer,
 * enabling efficient zero-copy operations and batch deallocation.
 */
typedef struct {
    uint8_t *buffer;        ///< Start of the arena buffer
    size_t size;           ///< Total size of the arena
    size_t used;           ///< Currently used bytes
    size_t alignment;      ///< Alignment requirement for allocations
} mp_memory_arena_t;

/**
 * @brief Initialize a memory view
 * 
 * @param view The memory view to initialize
 * @param data Pointer to the data buffer
 * @param length Length of the view
 * @param capacity Total capacity of the underlying buffer
 * @param offset Offset from the start of the underlying buffer
 * @param owns_data Whether this view owns the data
 * @param free_fn Function to free the data (if owns_data is true)
 * @return 0 on success, -1 on error
 */
int mp_memory_view_init(mp_memory_view_t *view, uint8_t *data, size_t length,
                        size_t capacity, size_t offset, bool owns_data,
                        void (*free_fn)(void*));

/**
 * @brief Create a memory view from a buffer
 * 
 * @param data Pointer to the data buffer
 * @param length Length of the view
 * @return Memory view (stack allocated, caller must manage lifetime)
 */
mp_memory_view_t mp_memory_view_from_buffer(uint8_t *data, size_t length);

/**
 * @brief Create a memory view that owns its data
 * 
 * @param data Pointer to the data buffer (will be freed with free_fn)
 * @param length Length of the view
 * @param free_fn Function to free the data
 * @return Memory view
 */
mp_memory_view_t mp_memory_view_owning(uint8_t *data, size_t length, void (*free_fn)(void*));

/**
 * @brief Free a memory view
 * 
 * Only frees the underlying data if owns_data is true.
 * 
 * @param view The memory view to free
 */
void mp_memory_view_free(mp_memory_view_t *view);

/**
 * @brief Create a slice of a memory view
 * 
 * Creates a new view that references a portion of the original view.
 * This is a zero-copy operation.
 * 
 * @param view The original memory view
 * @param start Start offset in bytes
 * @param length Length of the slice in bytes
 * @return New memory view referencing the slice
 */
mp_memory_view_t mp_memory_view_slice(mp_memory_view_t *view, size_t start, size_t length);

/**
 * @brief Create a typed slice from a memory view
 * 
 * @param view The memory view to slice
 * @param element_size Size of each element
 * @param element_count Number of elements
 * @return Typed memory slice
 */
mp_memory_slice_t mp_memory_view_as_slice(mp_memory_view_t *view, size_t element_size, size_t element_count);

/**
 * @brief Initialize a memory arena
 * 
 * @param arena The arena to initialize
 * @param buffer The buffer to use for allocations
 * @param size Size of the buffer
 * @param alignment Alignment requirement for allocations
 * @return 0 on success, -1 on error
 */
int mp_memory_arena_init(mp_memory_arena_t *arena, uint8_t *buffer, size_t size, size_t alignment);

/**
 * @brief Allocate memory from an arena
 * 
 * @param arena The arena to allocate from
 * @param size Number of bytes to allocate
 * @param alignment Alignment requirement
 * @return Pointer to allocated memory, or NULL on failure
 */
void *mp_memory_arena_alloc(mp_memory_arena_t *arena, size_t size, size_t alignment);

/**
 * @brief Allocate and zero-initialize memory from an arena
 * 
 * @param arena The arena to allocate from
 * @param size Number of bytes to allocate
 * @param alignment Alignment requirement
 * @return Pointer to allocated and zeroed memory, or NULL on failure
 */
void *mp_memory_arena_zalloc(mp_memory_arena_t *arena, size_t size, size_t alignment);

/**
 * @brief Reset an arena, freeing all allocations
 * 
 * @param arena The arena to reset
 */
void mp_memory_arena_reset(mp_memory_arena_t *arena);

/**
 * @brief Get remaining space in an arena
 * 
 * @param arena The arena to query
 * @return Remaining bytes available
 */
size_t mp_memory_arena_remaining(mp_memory_arena_t *arena);

/**
 * @brief Get used space in an arena
 * 
 * @param arena The arena to query
 * @return Used bytes
 */
size_t mp_memory_arena_used(mp_memory_arena_t *arena);

/**
 * @brief Check if a pointer is within an arena
 * 
 * @param arena The arena to check
 * @param ptr The pointer to verify
 * @return true if the pointer is within the arena's buffer
 */
bool mp_memory_arena_contains(mp_memory_arena_t *arena, const void *ptr);

/**
 * @brief Create a memory view from an arena allocation
 * 
 * @param arena The arena that owns the allocation
 * @param size Size of the view
 * @param alignment Alignment requirement
 * @return Memory view referencing the arena allocation
 */
mp_memory_view_t mp_memory_arena_alloc_view(mp_memory_arena_t *arena, size_t size, size_t alignment);

/**
 * @brief Initialize a typed memory slice
 * 
 * @param slice The slice to initialize
 * @param data Pointer to the data
 * @param element_size Size of each element
 * @param element_count Number of elements
 * @param capacity Total capacity in elements
 * @param owns_data Whether the slice owns the data
 * @param free_fn Function to free the data
 */
void mp_memory_slice_init(mp_memory_slice_t *slice, void *data, size_t element_size,
                         size_t element_count, size_t capacity, bool owns_data,
                         void (*free_fn)(void*));

/**
 * @brief Free a memory slice
 * 
 * @param slice The slice to free
 */
void mp_memory_slice_free(mp_memory_slice_t *slice);

/**
 * @brief Get element at index from a slice
 * 
 * @param slice The slice
 * @param index The index
 * @return Pointer to the element, or NULL if out of bounds
 */
void *mp_memory_slice_get(mp_memory_slice_t *slice, size_t index);

/**
 * @brief Get a sub-slice of a memory slice
 * 
 * @param slice The original slice
 * @param start Start index
 * @param count Number of elements
 * @return New slice referencing the sub-range
 */
mp_memory_slice_t mp_memory_slice_subslice(mp_memory_slice_t *slice, size_t start, size_t count);

/**
 * @brief Convert a memory slice to a memory view
 * 
 * @param slice The slice to convert
 * @return Memory view
 */
mp_memory_view_t mp_memory_slice_as_view(mp_memory_slice_t *slice);

/**
 * @brief Calculate the size needed for a memory view struct
 * 
 * Useful for embedding memory views in other structures.
 * 
 * @return Size of mp_memory_view_t
 */
static inline size_t mp_memory_view_sizeof(void) {
    return sizeof(mp_memory_view_t);
}

/**
 * @brief Calculate the size needed for a memory slice struct
 * 
 * @return Size of mp_memory_slice_t
 */
static inline size_t mp_memory_slice_sizeof(void) {
    return sizeof(mp_memory_slice_t);
}

/**
 * @brief Check if a memory view is valid
 * 
 * @param view The view to check
 * @return true if the view has valid data pointer and length
 */
static inline bool mp_memory_view_valid(mp_memory_view_t *view) {
    return view != NULL && view->data != NULL && view->length > 0;
}

/**
 * @brief Check if a memory slice is valid
 * 
 * @param slice The slice to check
 * @return true if the slice has valid data pointer and element count
 */
static inline bool mp_memory_slice_valid(mp_memory_slice_t *slice) {
    return slice != NULL && slice->data != NULL && slice->element_count > 0;
}

/**
 * @brief Get the end pointer of a memory view
 * 
 * @param view The memory view
 * @return Pointer to one past the end of the view
 */
static inline uint8_t *mp_memory_view_end(mp_memory_view_t *view) {
    return view->data + view->length;
}

/**
 * @brief Get the end pointer of a memory slice
 * 
 * @param slice The memory slice
 * @return Pointer to one past the end of the slice
 */
static inline void *mp_memory_slice_end(mp_memory_slice_t *slice) {
    return (uint8_t *)slice->data + (slice->element_count * slice->element_size);
}

/**
 * @brief Check if two memory views overlap
 * 
 * @param a First view
 * @param b Second view
 * @return true if the views overlap
 */
bool mp_memory_view_overlaps(mp_memory_view_t *a, mp_memory_view_t *b);

/**
 * @brief Copy data from one view to another
 * 
 * @param dest Destination view (must have enough capacity)
 * @param src Source view
 * @return Number of bytes copied, or -1 on error
 */
intptr_t mp_memory_view_copy(mp_memory_view_t *dest, mp_memory_view_t *src);

/**
 * @brief Compare two memory views
 * 
 * @param a First view
 * @param b Second view
 * @return 0 if equal, <0 if a < b, >0 if a > b
 */
int mp_memory_view_compare(mp_memory_view_t *a, mp_memory_view_t *b);

/**
 * @brief Find a byte in a memory view
 * 
 * @param view The view to search
 * @param byte The byte to find
 * @return Offset of the first occurrence, or -1 if not found
 */
intptr_t mp_memory_view_find_byte(mp_memory_view_t *view, uint8_t byte);

/**
 * @brief Find a pattern in a memory view
 * 
 * @param view The view to search
 * @param pattern The pattern to find
 * @param pattern_len Length of the pattern
 * @return Offset of the first occurrence, or -1 if not found
 */
intptr_t mp_memory_view_find_pattern(mp_memory_view_t *view, const uint8_t *pattern, size_t pattern_len);

#endif // MICROPOSIX_MEMORY_VIEW_H
