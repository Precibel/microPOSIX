#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "microposix/mm/tlsf.h"

// Simplified TLSF-like allocator
// For full implementation, we'd need bit-mapping of free blocks
// In this simulation, we'll use a simple first-fit for brevity

typedef struct block_header {
    size_t size;
    struct block_header *next;
    uint32_t thread_id;
    void *caller_pc;
    uint32_t magic; // 0xDEAD1234
} block_header_t;

int mp_tlsf_init(mp_tlsf_t *tlsf, void *pool, size_t size) {
    if (!tlsf || !pool || size < sizeof(block_header_t)) return -1;
    
    tlsf->pool = pool;
    tlsf->size = size;
    
    block_header_t *first = (block_header_t *)pool;
    first->size = size - sizeof(block_header_t);
    first->next = NULL;
    first->magic = 0xDEAD1234;
    
    return 0;
}

void *mp_tlsf_alloc(mp_tlsf_t *tlsf, size_t size) {
    block_header_t *curr = (block_header_t *)tlsf->pool;
    
    while (curr) {
        if (curr->size >= size + sizeof(block_header_t)) {
            // Split the block
            block_header_t *new_block = (block_header_t *)((uint8_t *)curr + sizeof(block_header_t) + size);
            new_block->size = curr->size - size - sizeof(block_header_t);
            new_block->next = curr->next;
            new_block->magic = 0xDEAD1234;
            
            curr->size = size;
            curr->next = new_block;
            
            return (void *)((uint8_t *)curr + sizeof(block_header_t));
        }
        curr = curr->next;
    }
    
    return NULL;
}

void mp_tlsf_free(mp_tlsf_t *tlsf, void *ptr) {
    (void)tlsf;
    if (!ptr) return;
    
    block_header_t *header = (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
    if (header->magic != 0xDEAD1234) {
        // Corrupted header
        return;
    }
    
    // In this simple version, we don't merge free blocks. 
    // In real TLSF, we'd merge with neighbors.
}
