#ifndef MICROPOSIX_POOL_H
#define MICROPOSIX_POOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct mp_pool {
    void *buffer;
    size_t block_size;
    size_t num_blocks;
    uint32_t *bitmap;
    uint32_t free_blocks;
} mp_pool_t;

// Pool management functions
int mp_pool_init(mp_pool_t *pool, void *buffer, size_t block_size, size_t num_blocks);
void *mp_pool_alloc(mp_pool_t *pool);
void mp_pool_free(mp_pool_t *pool, void *ptr);

#endif // MICROPOSIX_POOL_H
