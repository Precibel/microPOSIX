#ifndef MICROPOSIX_TLSF_H
#define MICROPOSIX_TLSF_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    void *pool;
    size_t size;
    // TLSF specific structures
} mp_tlsf_t;

int mp_tlsf_init(mp_tlsf_t *tlsf, void *pool, size_t size);
void *mp_tlsf_alloc(mp_tlsf_t *tlsf, size_t size);
void mp_tlsf_free(mp_tlsf_t *tlsf, void *ptr);

#endif // MICROPOSIX_TLSF_H
