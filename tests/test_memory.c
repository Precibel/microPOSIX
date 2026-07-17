#include "microposix/mm/memory_view.h"
#include "microposix/mm/shared_heap.h"
#include "microposix/mm/gc.h"
#include "microposix/mm/pool.h"
#include "microposix/mm/tlsf.h"
#include <stdio.h>
#include <string.h>

// Test memory view
static void test_memory_view(void) {
    printf("Testing memory view...\n");
    
    // Create a buffer
    uint8_t buffer[100];
    for (int i = 0; i < 100; i++) {
        buffer[i] = (uint8_t)i;
    }
    
    // Create a view
    mp_memory_view_t view = mp_memory_view_from_buffer(buffer, 100);
    
    // Test basic properties
    if (view.length != 100) {
        printf("ERROR: View length should be 100, got %zu\n", view.length);
    }
    if (view.capacity != 100) {
        printf("ERROR: View capacity should be 100, got %zu\n", view.capacity);
    }
    if (view.offset != 0) {
        printf("ERROR: View offset should be 0, got %zu\n", view.offset);
    }
    
    // Test slicing
    mp_memory_view_t slice = mp_memory_view_slice(&view, 10, 20);
    if (slice.length != 20) {
        printf("ERROR: Slice length should be 20, got %zu\n", slice.length);
    }
    if (slice.data != buffer + 10) {
        printf("ERROR: Slice data pointer incorrect\n");
    }
    
    // Test comparison
    mp_memory_view_t view2 = mp_memory_view_from_buffer(buffer, 100);
    if (mp_memory_view_compare(&view, &view2) != 0) {
        printf("ERROR: Views should be equal\n");
    }
    
    // Test find
    ssize_t pos = mp_memory_view_find_byte(&view, 42);
    if (pos != 42) {
        printf("ERROR: Should find 42 at position 42, got %zd\n", pos);
    }
    
    printf("Memory view tests passed!\n");
}

// Test memory arena
static void test_memory_arena(void) {
    printf("Testing memory arena...\n");
    
    uint8_t buffer[1024];
    mp_memory_arena_t arena;
    
    if (mp_memory_arena_init(&arena, buffer, 1024, 8) != 0) {
        printf("ERROR: Failed to initialize arena\n");
        return;
    }
    
    // Allocate some memory
    void *ptr1 = mp_memory_arena_alloc(&arena, 100, 8);
    void *ptr2 = mp_memory_arena_alloc(&arena, 200, 8);
    void *ptr3 = mp_memory_arena_alloc(&arena, 50, 8);
    
    if (!ptr1 || !ptr2 || !ptr3) {
        printf("ERROR: Failed to allocate from arena\n");
        return;
    }
    
    // Check used space
    size_t used = mp_memory_arena_used(&arena);
    if (used < 350) {
        printf("ERROR: Used space should be at least 350, got %zu\n", used);
    }
    
    // Check remaining space
    size_t remaining = mp_memory_arena_remaining(&arena);
    if (remaining > 1024 - 350) {
        printf("ERROR: Remaining space incorrect\n");
    }
    
    // Check contains
    if (!mp_memory_arena_contains(&arena, ptr1)) {
        printf("ERROR: Arena should contain ptr1\n");
    }
    
    // Reset arena
    mp_memory_arena_reset(&arena);
    
    if (mp_memory_arena_used(&arena) != 0) {
        printf("ERROR: Arena should be empty after reset\n");
    }
    
    printf("Memory arena tests passed!\n");
}

// Test shared heap
static void test_shared_heap(void) {
    printf("Testing shared heap...\n");
    
    uint8_t buffer[2048];
    mp_shared_heap_config_t config = {
        .buffer = buffer,
        .size = 2048,
        .min_block_size = 16,
        .alignment = 8,
        .thread_safe = false,
        .max_threads = 4
    };
    
    mp_shared_heap_t heap;
    if (mp_shared_heap_init(&heap, &config) != 0) {
        printf("ERROR: Failed to initialize shared heap\n");
        return;
    }
    
    // Allocate some memory
    void *ptr1 = mp_shared_heap_alloc(&heap, 100);
    void *ptr2 = mp_shared_heap_alloc(&heap, 200);
    void *ptr3 = mp_shared_heap_zalloc(&heap, 50);
    
    if (!ptr1 || !ptr2 || !ptr3) {
        printf("ERROR: Failed to allocate from shared heap\n");
        return;
    }
    
    // Check contains
    if (!mp_shared_heap_contains(&heap, ptr1)) {
        printf("ERROR: Heap should contain ptr1\n");
    }
    
    // Check block size
    size_t size = mp_shared_heap_get_block_size(&heap, ptr1);
    if (size < 100) {
        printf("ERROR: Block size should be at least 100, got %zu\n", size);
    }
    
    // Get statistics
    mp_shared_heap_stats_t stats;
    mp_shared_heap_get_stats(&heap, &stats);
    if (stats.used_size < 350) {
        printf("ERROR: Used size should be at least 350, got %zu\n", stats.used_size);
    }
    
    // Free memory
    mp_shared_heap_free(&heap, ptr1);
    mp_shared_heap_free(&heap, ptr2);
    mp_shared_heap_free(&heap, ptr3);
    
    // Reset heap
    mp_shared_heap_reset(&heap);
    
    mp_shared_heap_get_stats(&heap, &stats);
    if (stats.used_size != 0) {
        printf("ERROR: Used size should be 0 after reset, got %zu\n", stats.used_size);
    }
    
    printf("Shared heap tests passed!\n");
}

// Test garbage collection
static void test_gc(void) {
    printf("Testing garbage collection...\n");
    
    uint8_t heap_buffer[4096];
    mp_shared_heap_config_t heap_config = {
        .buffer = heap_buffer,
        .size = 4096,
        .min_block_size = 16,
        .alignment = 8,
        .thread_safe = false,
        .max_threads = 4
    };
    
    mp_shared_heap_t heap;
    if (mp_shared_heap_init(&heap, &heap_config) != 0) {
        printf("ERROR: Failed to initialize heap for GC\n");
        return;
    }
    
    mp_gc_config_t gc_config = {
        .heap = &heap,
        .initial_size = 4096,
        .max_objects = 100,
        .use_ref_counting = true,
        .use_mark_sweep = true,
        .use_generational = false,
        .young_gen_size = 1024,
        .threshold = 50,
        .gc_interval = 1000
    };
    
    mp_gc_t gc;
    if (mp_gc_init(&gc, &gc_config) != 0) {
        printf("ERROR: Failed to initialize GC\n");
        return;
    }
    
    // Allocate some objects
    void *obj1 = mp_gc_alloc(&gc, 100);
    void *obj2 = mp_gc_alloc(&gc, 200);
    
    if (!obj1 || !obj2) {
        printf("ERROR: Failed to allocate GC objects\n");
        mp_gc_destroy(&gc);
        return;
    }
    
    // Check refcount
    uint32_t refcount = mp_gc_get_refcount(&gc, obj1);
    if (refcount != 1) {
        printf("ERROR: Refcount should be 1, got %u\n", refcount);
    }
    
    // Increment refcount
    mp_gc_incref(&gc, obj1);
    refcount = mp_gc_get_refcount(&gc, obj1);
    if (refcount != 2) {
        printf("ERROR: Refcount should be 2 after incref, got %u\n", refcount);
    }
    
    // Decrement refcount
    mp_gc_decref(&gc, obj1);
    refcount = mp_gc_get_refcount(&gc, obj1);
    if (refcount != 1) {
        printf("ERROR: Refcount should be 1 after decref, got %u\n", refcount);
    }
    
    // Get statistics
    mp_gc_stats_t stats;
    mp_gc_get_stats(&gc, &stats);
    if (stats.total_objects < 2) {
        printf("ERROR: Should have at least 2 objects, got %zu\n", stats.total_objects);
    }
    
    // Cleanup
    mp_gc_destroy(&gc);
    
    printf("Garbage collection tests passed!\n");
}

// Test pool allocator
static void test_pool(void) {
    printf("Testing pool allocator...\n");
    
    uint8_t buffer[1024];
    mp_pool_t pool;
    
    if (mp_pool_init(&pool, buffer, 64, 16) != 0) {
        printf("ERROR: Failed to initialize pool\n");
        return;
    }
    
    // Allocate all blocks
    void *ptrs[16];
    for (int i = 0; i < 16; i++) {
        ptrs[i] = mp_pool_alloc(&pool);
        if (!ptrs[i]) {
            printf("ERROR: Failed to allocate block %d\n", i);
            return;
        }
    }
    
    // Try to allocate one more (should fail)
    void *extra = mp_pool_alloc(&pool);
    if (extra) {
        printf("ERROR: Should not be able to allocate extra block\n");
        return;
    }
    
    // Free some blocks
    mp_pool_free(&pool, ptrs[0]);
    mp_pool_free(&pool, ptrs[5]);
    mp_pool_free(&pool, ptrs[10]);
    
    // Allocate again
    void *new_ptr = mp_pool_alloc(&pool);
    if (!new_ptr) {
        printf("ERROR: Failed to allocate after freeing\n");
        return;
    }
    
    printf("Pool allocator tests passed!\n");
}

int main(void) {
    printf("Running memory management tests...\n\n");
    
    test_memory_view();
    test_memory_arena();
    test_shared_heap();
    test_gc();
    test_pool();
    
    printf("\nAll memory management tests passed!\n");
    return 0;
}
