#include "microposix/mm/shared_heap.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Global heap instance
mp_shared_heap_t *mp_shared_heap_global = NULL;

// Helper to align a pointer
static inline uintptr_t align_up(uintptr_t ptr, size_t alignment) {
    return (ptr + alignment - 1) & ~(alignment - 1);
}

// Helper to get block header from pointer
static inline block_header_t *get_block_header(void *ptr) {
    return (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
}

// Helper to get data pointer from block header
static inline void *get_data_from_block(block_header_t *block) {
    return (void *)((uint8_t *)block + sizeof(block_header_t));
}

// Helper to get block size including header
static inline size_t get_block_total_size(block_header_t *block) {
    return block->size + sizeof(block_header_t);
}

// Lock heap if thread-safe
static inline void heap_lock(mp_shared_heap_t *heap) {
    if (heap && heap->thread_safe && heap->mutex) {
        // For now, just a placeholder - in real implementation, use mp_mutex_lock
        (void)heap;
    }
}

// Unlock heap if thread-safe
static inline void heap_unlock(mp_shared_heap_t *heap) {
    if (heap && heap->thread_safe && heap->mutex) {
        // For now, just a placeholder - in real implementation, use mp_mutex_unlock
        (void)heap;
    }
}

// Initialize block header
static void init_block_header(block_header_t *block, size_t size, bool is_free,
                              mp_thread_t *thread, const char *file, int line) {
    block->size = size;
    block->next = NULL;
    block->prev = NULL;
    block->magic = MP_SHARED_HEAP_MAGIC;
    block->is_free = is_free;
    block->thread = thread;
    block->file = file;
    block->line = line;
}

// Split a block if there's enough space left
static block_header_t *split_block(block_header_t *block, size_t requested_size) {
    size_t remaining = block->size - requested_size - sizeof(block_header_t);
    
    if (remaining >= sizeof(block_header_t) + 8) { // Minimum useful block size
        block_header_t *new_block = (block_header_t *)((uint8_t *)block + sizeof(block_header_t) + requested_size);
        
        // Initialize new block as free
        init_block_header(new_block, remaining, true, NULL, NULL, 0);
        
        // Update original block
        block->size = requested_size;
        
        return new_block;
    }
    
    return NULL;
}

// Merge adjacent free blocks
static void merge_free_blocks(mp_shared_heap_t *heap, block_header_t *block) {
    block_header_t *current = heap->free_list;
    
    while (current) {
        // Check if current block is adjacent to the block we're freeing
        uintptr_t current_end = (uintptr_t)current + sizeof(block_header_t) + current->size;
        uintptr_t block_start = (uintptr_t)block;
        uintptr_t block_end = (uintptr_t)block + sizeof(block_header_t) + block->size;
        
        if (current_end == block_start) {
            // Merge current with block
            current->size += sizeof(block_header_t) + block->size;
            
            // Check if block is also adjacent to current's next
            if (current->next && (uintptr_t)current->next == block_end) {
                current->size += sizeof(block_header_t) + current->next->size;
                current->next = current->next->next;
                if (current->next) {
                    current->next->prev = current;
                }
            }
            return;
        } else if (block_end == (uintptr_t)current) {
            // Merge block with current
            block->size += sizeof(block_header_t) + current->size;
            block->next = current->next;
            if (current->next) {
                current->next->prev = block;
            }
            
            // Remove current from free list
            if (current->prev) {
                current->prev->next = current->next;
            } else {
                heap->free_list = current->next;
            }
            
            // Add block to free list
            block->prev = current->prev;
            if (current->prev) {
                current->prev->next = block;
            } else {
                heap->free_list = block;
            }
            return;
        }
        
        current = current->next;
    }
}

// Add block to free list (maintaining sorted order by address)
static void add_to_free_list(mp_shared_heap_t *heap, block_header_t *block) {
    block->is_free = true;
    block->next = NULL;
    block->prev = NULL;
    
    if (!heap->free_list) {
        heap->free_list = block;
        return;
    }
    
    // Insert in address order
    block_header_t *current = heap->free_list;
    block_header_t *prev = NULL;
    
    while (current && (uintptr_t)current < (uintptr_t)block) {
        prev = current;
        current = current->next;
    }
    
    if (prev) {
        prev->next = block;
        block->prev = prev;
    } else {
        heap->free_list = block;
    }
    
    block->next = current;
    if (current) {
        current->prev = block;
    }
}

// Remove block from free list
static void remove_from_free_list(mp_shared_heap_t *heap, block_header_t *block) {
    if (block->prev) {
        block->prev->next = block->next;
    } else {
        heap->free_list = block->next;
    }
    
    if (block->next) {
        block->next->prev = block->prev;
    }
    
    block->is_free = false;
    block->next = NULL;
    block->prev = NULL;
}

// Find a free block of at least the requested size
static block_header_t *find_free_block(mp_shared_heap_t *heap, size_t size, size_t alignment) {
    block_header_t *current = heap->free_list;
    block_header_t *best_fit = NULL;
    
    while (current) {
        // Calculate aligned address within this block
        uintptr_t block_addr = (uintptr_t)current + sizeof(block_header_t);
        uintptr_t aligned_addr = align_up(block_addr, alignment);
        size_t padding = aligned_addr - block_addr;
        
        // Check if block is large enough
        if (current->size >= size + padding) {
            if (!best_fit || current->size < best_fit->size) {
                best_fit = current;
            }
        }
        
        current = current->next;
    }
    
    return best_fit;
}

int mp_shared_heap_init(mp_shared_heap_t *heap, mp_shared_heap_config_t *config) {
    if (!heap || !config || !config->buffer || config->size == 0) {
        return -1;
    }
    
    // Initialize heap structure
    heap->buffer = config->buffer;
    heap->size = config->size;
    heap->used = 0;
    heap->peak_used = 0;
    heap->alignment = config->alignment > 0 ? config->alignment : 8;
    heap->thread_safe = config->thread_safe;
    heap->free_list = NULL;
    heap->impl_data = NULL;
    heap->allocations = NULL;
    heap->max_allocations = 0;
    heap->num_allocations = 0;
    heap->thread_caches = NULL;
    
    // Initialize statistics
    memset(&heap->stats, 0, sizeof(heap->stats));
    heap->stats.total_size = config->size;
    heap->stats.free_size = config->size;
    
    // Create mutex if thread-safe (placeholder for now)
    heap->mutex = NULL;
    
    // Initialize the entire buffer as one free block
    block_header_t *first_block = (block_header_t *)config->buffer;
    size_t block_size = config->size - sizeof(block_header_t);
    
    init_block_header(first_block, block_size, true, NULL, NULL, 0);
    heap->free_list = first_block;
    
    return 0;
}

void mp_shared_heap_destroy(mp_shared_heap_t *heap) {
    if (!heap) {
        return;
    }
    
    heap_lock(heap);
    
    // Free mutex
    if (heap->mutex) {
        // mp_mutex_destroy(heap->mutex);
        heap->mutex = NULL;
    }
    
    // Free allocation tracking array
    if (heap->allocations) {
        free(heap->allocations);
        heap->allocations = NULL;
    }
    
    // Free thread caches
    if (heap->thread_caches) {
        free(heap->thread_caches);
        heap->thread_caches = NULL;
    }
    
    heap_unlock(heap);
}

void *mp_shared_heap_alloc(mp_shared_heap_t *heap, size_t size) {
    return mp_shared_heap_aligned_alloc(heap, size, heap->alignment);
}

void *mp_shared_heap_aligned_alloc(mp_shared_heap_t *heap, size_t size, size_t alignment) {
    if (!heap || size == 0) {
        return NULL;
    }
    
    // Ensure alignment is a power of 2
    if ((alignment & (alignment - 1)) != 0) {
        return NULL;
    }
    
    heap_lock(heap);
    
    // Find a suitable free block
    block_header_t *block = find_free_block(heap, size, alignment);
    
    if (!block) {
        heap_unlock(heap);
        return NULL;
    }
    
    // Calculate aligned address and padding
    uintptr_t block_addr = (uintptr_t)block + sizeof(block_header_t);
    uintptr_t aligned_addr = align_up(block_addr, alignment);
    size_t padding = aligned_addr - block_addr;
    
    // Total size needed including header and padding
    size_t total_needed = size + padding;
    
    // Split block if there's enough space left
    block_header_t *new_free_block = split_block(block, total_needed);
    
    // Remove block from free list
    remove_from_free_list(heap, block);
    
    // Update heap statistics
    heap->used += sizeof(block_header_t) + total_needed;
    if (heap->used > heap->peak_used) {
        heap->peak_used = heap->used;
    }
    heap->stats.used_size = heap->used;
    heap->stats.free_size = heap->size - heap->used;
    heap->stats.num_allocations++;
    
    // Add new free block to free list if we split
    if (new_free_block) {
        add_to_free_list(heap, new_free_block);
    }
    
    // Initialize block header
    block->size = total_needed;
    block->magic = MP_SHARED_HEAP_MAGIC;
    block->is_free = false;
    block->thread = NULL; // Would be mp_thread_current() in real implementation
    block->file = NULL;
    block->line = 0;
    
    heap_unlock(heap);
    
    return (void *)aligned_addr;
}

void *mp_shared_heap_zalloc(mp_shared_heap_t *heap, size_t size) {
    void *ptr = mp_shared_heap_alloc(heap, size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void mp_shared_heap_free(mp_shared_heap_t *heap, void *ptr) {
    mp_shared_heap_free_tracked(heap, ptr, NULL, 0);
}

void mp_shared_heap_free_tracked(mp_shared_heap_t *heap, void *ptr, const char *file, int line) {
    (void)file;
    (void)line;
    
    if (!heap || !ptr) {
        return;
    }
    
    heap_lock(heap);
    
    // Get block header
    block_header_t *block = get_block_header(ptr);
    
    // Validate magic number
    if (block->magic != MP_SHARED_HEAP_MAGIC || block->is_free) {
        heap_unlock(heap);
        return; // Invalid block or double free
    }
    
    // Update statistics
    heap->used -= sizeof(block_header_t) + block->size;
    heap->stats.used_size = heap->used;
    heap->stats.free_size = heap->size - heap->used;
    heap->stats.num_frees++;
    
    // Add to free list and merge with adjacent blocks
    add_to_free_list(heap, block);
    merge_free_blocks(heap, block);
    
    heap_unlock(heap);
}

void *mp_shared_heap_realloc(mp_shared_heap_t *heap, void *ptr, size_t new_size) {
    if (!heap) {
        return NULL;
    }
    
    if (!ptr) {
        return mp_shared_heap_alloc(heap, new_size);
    }
    
    heap_lock(heap);
    
    block_header_t *block = get_block_header(ptr);
    
    // Validate block
    if (block->magic != MP_SHARED_HEAP_MAGIC || block->is_free) {
        heap_unlock(heap);
        return NULL;
    }
    
    // If new size is smaller or equal, we can just return the same pointer
    if (new_size <= block->size) {
        heap->stats.num_reallocs++;
        heap_unlock(heap);
        return ptr;
    }
    
    // Allocate new block
    void *new_ptr = mp_shared_heap_alloc(heap, new_size);
    if (!new_ptr) {
        heap_unlock(heap);
        return NULL;
    }
    
    // Copy old data
    size_t copy_size = block->size < new_size ? block->size : new_size;
    memcpy(new_ptr, ptr, copy_size);
    
    // Free old block
    mp_shared_heap_free(heap, ptr);
    
    heap->stats.num_reallocs++;
    heap_unlock(heap);
    
    return new_ptr;
}

void *mp_shared_heap_alloc_tracked(mp_shared_heap_t *heap, size_t size, const char *file, int line) {
    if (!heap || size == 0) {
        return NULL;
    }
    
    heap_lock(heap);
    
    // Allocate the block
    void *ptr = mp_shared_heap_aligned_alloc(heap, size, heap->alignment);
    
    if (ptr) {
        block_header_t *block = get_block_header(ptr);
        block->file = file;
        block->line = line;
        
        // Track allocation if enabled
        if (heap->allocations && heap->num_allocations < heap->max_allocations) {
            heap->allocations[heap->num_allocations].ptr = ptr;
            heap->allocations[heap->num_allocations].size = size;
            heap->allocations[heap->num_allocations].alignment = heap->alignment;
            heap->allocations[heap->num_allocations].thread = NULL; // Would be mp_thread_current()
            heap->allocations[heap->num_allocations].file = file;
            heap->allocations[heap->num_allocations].line = line;
            heap->allocations[heap->num_allocations].timestamp = 0;
            heap->num_allocations++;
        }
    }
    
    heap_unlock(heap);
    
    return ptr;
}

void mp_shared_heap_get_stats(mp_shared_heap_t *heap, mp_shared_heap_stats_t *stats) {
    if (!heap || !stats) {
        return;
    }
    
    heap_lock(heap);
    memcpy(stats, &heap->stats, sizeof(*stats));
    heap_unlock(heap);
}

void mp_shared_heap_reset(mp_shared_heap_t *heap) {
    if (!heap) {
        return;
    }
    
    heap_lock(heap);
    
    // Reset statistics
    heap->used = 0;
    heap->peak_used = 0;
    heap->stats.used_size = 0;
    heap->stats.free_size = heap->size;
    heap->stats.num_allocations = 0;
    heap->stats.num_frees = 0;
    heap->stats.num_reallocs = 0;
    heap->num_allocations = 0;
    
    // Reset free list to one big block
    block_header_t *first_block = (block_header_t *)heap->buffer;
    size_t block_size = heap->size - sizeof(block_header_t);
    
    init_block_header(first_block, block_size, true, NULL, NULL, 0);
    heap->free_list = first_block;
    
    heap_unlock(heap);
}

bool mp_shared_heap_contains(mp_shared_heap_t *heap, const void *ptr) {
    if (!heap || !ptr) {
        return false;
    }
    
    uintptr_t ptr_val = (uintptr_t)ptr;
    uintptr_t start = (uintptr_t)heap->buffer;
    uintptr_t end = start + heap->size;
    
    return ptr_val >= start && ptr_val < end;
}

size_t mp_shared_heap_get_block_size(mp_shared_heap_t *heap, const void *ptr) {
    if (!heap || !ptr) {
        return 0;
    }
    
    block_header_t *block = (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
    
    if (block->magic == MP_SHARED_HEAP_MAGIC && !block->is_free) {
        return block->size;
    }
    
    return 0;
}

int mp_shared_heap_enable_tracking(mp_shared_heap_t *heap, size_t max_allocations) {
    if (!heap || max_allocations == 0) {
        return -1;
    }
    
    heap_lock(heap);
    
    heap->allocations = (mp_allocation_info_t *)malloc(max_allocations * sizeof(mp_allocation_info_t));
    if (!heap->allocations) {
        heap_unlock(heap);
        return -1;
    }
    
    heap->max_allocations = max_allocations;
    heap->num_allocations = 0;
    
    heap_unlock(heap);
    
    return 0;
}

void mp_shared_heap_disable_tracking(mp_shared_heap_t *heap) {
    if (!heap) {
        return;
    }
    
    heap_lock(heap);
    
    if (heap->allocations) {
        free(heap->allocations);
        heap->allocations = NULL;
    }
    heap->max_allocations = 0;
    heap->num_allocations = 0;
    
    heap_unlock(heap);
}

int mp_shared_heap_get_allocation_info(mp_shared_heap_t *heap, size_t index, mp_allocation_info_t *info) {
    if (!heap || !info || index >= heap->num_allocations) {
        return -1;
    }
    
    heap_lock(heap);
    memcpy(info, &heap->allocations[index], sizeof(*info));
    heap_unlock(heap);
    
    return 0;
}

void mp_shared_heap_dump(mp_shared_heap_t *heap, mp_heap_dump_callback_t callback, void *user_data) {
    if (!heap || !callback) {
        return;
    }
    
    char line[256];
    mp_shared_heap_stats_t stats;
    
    heap_lock(heap);
    
    mp_shared_heap_get_stats(heap, &stats);
    
    snprintf(line, sizeof(line), "Shared Heap Dump:");
    callback(line, user_data);
    
    snprintf(line, sizeof(line), "  Total Size: %zu bytes", stats.total_size);
    callback(line, user_data);
    
    snprintf(line, sizeof(line), "  Used: %zu bytes (%zu%%)", stats.used_size, 
             stats.total_size > 0 ? (stats.used_size * 100) / stats.total_size : 0);
    callback(line, user_data);
    
    snprintf(line, sizeof(line), "  Free: %zu bytes (%zu%%)", stats.free_size,
             stats.total_size > 0 ? (stats.free_size * 100) / stats.total_size : 0);
    callback(line, user_data);
    
    snprintf(line, sizeof(line), "  Peak Used: %zu bytes", stats.peak_used);
    callback(line, user_data);
    
    snprintf(line, sizeof(line), "  Allocations: %zu", stats.num_allocations);
    callback(line, user_data);
    
    snprintf(line, sizeof(line), "  Frees: %zu", stats.num_frees);
    callback(line, user_data);
    
    snprintf(line, sizeof(line), "  Reallocs: %zu", stats.num_reallocs);
    callback(line, user_data);
    
    snprintf(line, sizeof(line), "  Largest Free Block: %zu bytes", stats.largest_free);
    callback(line, user_data);
    
    snprintf(line, sizeof(line), "  Fragmentation: %zu%%", stats.fragmentation);
    callback(line, user_data);
    
    // Dump free list
    snprintf(line, sizeof(line), "  Free Blocks:");
    callback(line, user_data);
    
    block_header_t *block = heap->free_list;
    int count = 0;
    while (block) {
        snprintf(line, sizeof(line), "    Block %d: %zu bytes at %p", 
                 count++, block->size, (void *)block);
        callback(line, user_data);
        block = block->next;
    }
    
    // Dump tracked allocations if enabled
    if (heap->allocations && heap->num_allocations > 0) {
        snprintf(line, sizeof(line), "  Tracked Allocations:");
        callback(line, user_data);
        
        for (size_t i = 0; i < heap->num_allocations; i++) {
            mp_allocation_info_t *info = &heap->allocations[i];
            snprintf(line, sizeof(line), "    %zu: %zu bytes at %p (thread: %p, %s:%d)",
                     i, info->size, info->ptr, (void *)info->thread,
                     info->file ? info->file : "unknown", info->line);
            callback(line, user_data);
        }
    }
    
    heap_unlock(heap);
}

bool mp_shared_heap_validate(mp_shared_heap_t *heap) {
    if (!heap) {
        return false;
    }
    
    heap_lock(heap);
    
    bool valid = true;
    block_header_t *block = (block_header_t *)heap->buffer;
    uintptr_t end = (uintptr_t)heap->buffer + heap->size;
    
    while ((uintptr_t)block < end) {
        // Check magic number
        if (block->magic != MP_SHARED_HEAP_MAGIC) {
            valid = false;
            break;
        }
        
        // Check block size
        if (block->size == 0) {
            valid = false;
            break;
        }
        
        // Move to next block
        block = (block_header_t *)((uint8_t *)block + sizeof(block_header_t) + block->size);
    }
    
    heap_unlock(heap);
    
    return valid;
}

size_t mp_shared_heap_defrag(mp_shared_heap_t *heap) {
    if (!heap) {
        return 0;
    }
    
    heap_lock(heap);
    
    size_t coalesced = 0;
    block_header_t *current = heap->free_list;
    
    while (current) {
        block_header_t *next = current->next;
        
        // Try to merge with next block
        if (next && (uintptr_t)current + sizeof(block_header_t) + current->size == (uintptr_t)next) {
            current->size += sizeof(block_header_t) + next->size;
            current->next = next->next;
            if (next->next) {
                next->next->prev = current;
            }
            coalesced++;
            // Don't advance - check if we can merge again
            continue;
        }
        
        current = next;
    }
    
    heap_unlock(heap);
    
    return coalesced;
}

int mp_shared_heap_create_thread_cache(mp_shared_heap_t *heap, mp_thread_t *thread) {
    (void)heap;
    (void)thread;
    // TODO: Implement thread-local caching
    return 0;
}

void mp_shared_heap_destroy_thread_cache(mp_shared_heap_t *heap, mp_thread_t *thread) {
    (void)heap;
    (void)thread;
    // TODO: Implement thread-local caching
}

// Global heap functions
int mp_shared_heap_global_init(void *buffer, size_t size) {
    if (mp_shared_heap_global) {
        return -1; // Already initialized
    }
    
    mp_shared_heap_global = (mp_shared_heap_t *)malloc(sizeof(mp_shared_heap_t));
    if (!mp_shared_heap_global) {
        return -1;
    }
    
    mp_shared_heap_config_t config = {
        .buffer = buffer,
        .size = size,
        .min_block_size = 16,
        .alignment = 8,
        .thread_safe = true,
        .max_threads = 32
    };
    
    return mp_shared_heap_init(mp_shared_heap_global, &config);
}

void *mp_shared_heap_global_alloc(size_t size) {
    if (!mp_shared_heap_global) {
        return NULL;
    }
    return mp_shared_heap_alloc(mp_shared_heap_global, size);
}

void mp_shared_heap_global_free(void *ptr) {
    if (!mp_shared_heap_global) {
        return;
    }
    mp_shared_heap_free(mp_shared_heap_global, ptr);
}

void *mp_shared_heap_global_alloc_tracked(size_t size, const char *file, int line) {
    if (!mp_shared_heap_global) {
        return NULL;
    }
    return mp_shared_heap_alloc_tracked(mp_shared_heap_global, size, file, line);
}

void mp_shared_heap_global_free_tracked(void *ptr, const char *file, int line) {
    if (!mp_shared_heap_global) {
        return;
    }
    mp_shared_heap_free_tracked(mp_shared_heap_global, ptr, file, line);
}

void mp_shared_heap_global_get_stats(mp_shared_heap_stats_t *stats) {
    if (!mp_shared_heap_global) {
        if (stats) {
            memset(stats, 0, sizeof(*stats));
        }
        return;
    }
    mp_shared_heap_get_stats(mp_shared_heap_global, stats);
}

bool mp_shared_heap_global_contains(const void *ptr) {
    if (!mp_shared_heap_global) {
        return false;
    }
    return mp_shared_heap_contains(mp_shared_heap_global, ptr);
}
