#include <stdint.h>
#include <string.h>
#include "microposix/mm/pool.h"

int mp_pool_init(mp_pool_t *pool, void *buffer, size_t block_size, size_t num_blocks) {
    if (!pool || !buffer) return -1;
    
    pool->buffer = buffer;
    pool->block_size = block_size;
    pool->num_blocks = num_blocks;
    pool->free_blocks = num_blocks;
    
    // Allocate bitmap (simplification: assume user provides enough space or we allocate from heap)
    // For now, let's just use a simple linked list of blocks
    uintptr_t *ptr = (uintptr_t *)buffer;
    for (size_t i = 0; i < num_blocks - 1; i++) {
        *ptr = (uintptr_t)((uint8_t *)ptr + block_size);
        ptr = (uintptr_t *)*ptr;
    }
    *ptr = (uintptr_t)NULL;
    
    // Store the first free block in the bitmap field (as a pointer for now)
    pool->bitmap = (uint32_t *)buffer;
    
    return 0;
}

void *mp_pool_alloc(mp_pool_t *pool) {
    if (pool->free_blocks == 0) return NULL;
    
    void *block = (void *)pool->bitmap;
    pool->bitmap = (uint32_t *)(*(uintptr_t *)block);
    pool->free_blocks--;
    
    return block;
}

void mp_pool_free(mp_pool_t *pool, void *ptr) {
    if (!pool || !ptr) return;
    
    *(uintptr_t *)ptr = (uintptr_t)pool->bitmap;
    pool->bitmap = (uint32_t *)ptr;
    pool->free_blocks++;
}
