#include "microposix/mm/memory_view.h"
#include <string.h>
#include <stdint.h>
#include <stddef.h>

// Helper to align a pointer
static inline uintptr_t align_up(uintptr_t ptr, size_t alignment) {
    return (ptr + alignment - 1) & ~(alignment - 1);
}

// Helper to align a size
static inline size_t align_up_size(size_t size, size_t alignment) {
    return (size + alignment - 1) & ~(alignment - 1);
}

int mp_memory_view_init(mp_memory_view_t *view, uint8_t *data, size_t length,
                        size_t capacity, size_t offset, bool owns_data,
                        void (*free_fn)(void*)) {
    if (!view || (!data && length > 0)) {
        return -1;
    }
    
    view->data = data;
    view->length = length;
    view->capacity = capacity;
    view->offset = offset;
    view->owns_data = owns_data;
    view->free_fn = free_fn;
    
    return 0;
}

mp_memory_view_t mp_memory_view_from_buffer(uint8_t *data, size_t length) {
    mp_memory_view_t view;
    mp_memory_view_init(&view, data, length, length, 0, false, NULL);
    return view;
}

mp_memory_view_t mp_memory_view_owning(uint8_t *data, size_t length, void (*free_fn)(void*)) {
    mp_memory_view_t view;
    mp_memory_view_init(&view, data, length, length, 0, true, free_fn);
    return view;
}

void mp_memory_view_free(mp_memory_view_t *view) {
    if (view && view->owns_data && view->data && view->free_fn) {
        view->free_fn(view->data);
        view->data = NULL;
        view->length = 0;
        view->capacity = 0;
        view->owns_data = false;
    }
}

mp_memory_view_t mp_memory_view_slice(mp_memory_view_t *view, size_t start, size_t length) {
    mp_memory_view_t slice;
    
    if (!view || start >= view->length) {
        slice.data = NULL;
        slice.length = 0;
        slice.capacity = 0;
        slice.offset = 0;
        slice.owns_data = false;
        slice.free_fn = NULL;
        return slice;
    }
    
    // Clamp length to available data
    if (start + length > view->length) {
        length = view->length - start;
    }
    
    slice.data = view->data + start;
    slice.length = length;
    slice.capacity = view->capacity - start;
    slice.offset = view->offset + start;
    slice.owns_data = false; // Slices never own data
    slice.free_fn = NULL;
    
    return slice;
}

mp_memory_slice_t mp_memory_view_as_slice(mp_memory_view_t *view, size_t element_size, size_t element_count) {
    mp_memory_slice_t slice;
    
    if (!view || element_size == 0) {
        slice.data = NULL;
        slice.element_size = 0;
        slice.element_count = 0;
        slice.capacity = 0;
        slice.owns_data = false;
        slice.free_fn = NULL;
        return slice;
    }
    
    // Calculate how many elements we can actually have
    size_t max_elements = view->length / element_size;
    if (element_count > max_elements) {
        element_count = max_elements;
    }
    
    slice.data = view->data;
    slice.element_size = element_size;
    slice.element_count = element_count;
    slice.capacity = max_elements;
    slice.owns_data = false;
    slice.free_fn = NULL;
    
    return slice;
}

int mp_memory_arena_init(mp_memory_arena_t *arena, uint8_t *buffer, size_t size, size_t alignment) {
    if (!arena || !buffer || size == 0) {
        return -1;
    }
    
    // Ensure alignment is a power of 2
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        return -1;
    }
    
    arena->buffer = buffer;
    arena->size = size;
    arena->used = 0;
    arena->alignment = alignment;
    
    return 0;
}

void *mp_memory_arena_alloc(mp_memory_arena_t *arena, size_t size, size_t alignment) {
    if (!arena || size == 0) {
        return NULL;
    }
    
    // Use the arena's alignment if not specified
    if (alignment == 0) {
        alignment = arena->alignment;
    }
    
    // Ensure alignment is a power of 2
    if ((alignment & (alignment - 1)) != 0) {
        return NULL;
    }
    
    // Calculate aligned offset
    uintptr_t current_ptr = (uintptr_t)(arena->buffer + arena->used);
    uintptr_t aligned_ptr = align_up(current_ptr, alignment);
    size_t padding = aligned_ptr - current_ptr;
    
    // Check if we have enough space
    size_t total_needed = padding + size;
    if (arena->used + total_needed > arena->size) {
        return NULL;
    }
    
    // Update used count
    arena->used += total_needed;
    
    return (void *)aligned_ptr;
}

void *mp_memory_arena_zalloc(mp_memory_arena_t *arena, size_t size, size_t alignment) {
    void *ptr = mp_memory_arena_alloc(arena, size, alignment);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void mp_memory_arena_reset(mp_memory_arena_t *arena) {
    if (arena) {
        arena->used = 0;
    }
}

size_t mp_memory_arena_remaining(mp_memory_arena_t *arena) {
    if (!arena) {
        return 0;
    }
    return arena->size - arena->used;
}

size_t mp_memory_arena_used(mp_memory_arena_t *arena) {
    if (!arena) {
        return 0;
    }
    return arena->used;
}

bool mp_memory_arena_contains(mp_memory_arena_t *arena, const void *ptr) {
    if (!arena || !ptr) {
        return false;
    }
    
    uintptr_t ptr_val = (uintptr_t)ptr;
    uintptr_t start = (uintptr_t)arena->buffer;
    uintptr_t end = start + arena->size;
    
    return ptr_val >= start && ptr_val < end;
}

mp_memory_view_t mp_memory_arena_alloc_view(mp_memory_arena_t *arena, size_t size, size_t alignment) {
    void *ptr = mp_memory_arena_alloc(arena, size, alignment);
    if (!ptr) {
        mp_memory_view_t empty;
        empty.data = NULL;
        empty.length = 0;
        empty.capacity = 0;
        empty.offset = 0;
        empty.owns_data = false;
        empty.free_fn = NULL;
        return empty;
    }
    
    mp_memory_view_t view;
    mp_memory_view_init(&view, (uint8_t *)ptr, size, size, (size_t)((uint8_t *)ptr - arena->buffer), false, NULL);
    return view;
}

void mp_memory_slice_init(mp_memory_slice_t *slice, void *data, size_t element_size,
                         size_t element_count, size_t capacity, bool owns_data,
                         void (*free_fn)(void*)) {
    if (!slice) {
        return;
    }
    
    slice->data = data;
    slice->element_size = element_size;
    slice->element_count = element_count;
    slice->capacity = capacity;
    slice->owns_data = owns_data;
    slice->free_fn = free_fn;
}

void mp_memory_slice_free(mp_memory_slice_t *slice) {
    if (slice && slice->owns_data && slice->data && slice->free_fn) {
        slice->free_fn(slice->data);
        slice->data = NULL;
        slice->element_count = 0;
        slice->capacity = 0;
        slice->owns_data = false;
    }
}

void *mp_memory_slice_get(mp_memory_slice_t *slice, size_t index) {
    if (!slice || index >= slice->element_count) {
        return NULL;
    }
    return (uint8_t *)slice->data + (index * slice->element_size);
}

mp_memory_slice_t mp_memory_slice_subslice(mp_memory_slice_t *slice, size_t start, size_t count) {
    mp_memory_slice_t subslice;
    
    if (!slice || slice->element_size == 0 || start >= slice->element_count) {
        subslice.data = NULL;
        subslice.element_size = 0;
        subslice.element_count = 0;
        subslice.capacity = 0;
        subslice.owns_data = false;
        subslice.free_fn = NULL;
        return subslice;
    }
    
    // Clamp count
    if (start + count > slice->element_count) {
        count = slice->element_count - start;
    }
    
    subslice.data = (uint8_t *)slice->data + (start * slice->element_size);
    subslice.element_size = slice->element_size;
    subslice.element_count = count;
    subslice.capacity = slice->capacity - start;
    subslice.owns_data = false;
    subslice.free_fn = NULL;
    
    return subslice;
}

mp_memory_view_t mp_memory_slice_as_view(mp_memory_slice_t *slice) {
    if (!slice) {
        mp_memory_view_t empty;
        empty.data = NULL;
        empty.length = 0;
        empty.capacity = 0;
        empty.offset = 0;
        empty.owns_data = false;
        empty.free_fn = NULL;
        return empty;
    }
    
    mp_memory_view_t view;
    view.data = (uint8_t *)slice->data;
    view.length = slice->element_count * slice->element_size;
    view.capacity = slice->capacity * slice->element_size;
    view.offset = 0;
    view.owns_data = false;
    view.free_fn = NULL;
    
    return view;
}

bool mp_memory_view_overlaps(mp_memory_view_t *a, mp_memory_view_t *b) {
    if (!a || !b || !a->data || !b->data) {
        return false;
    }
    
    uintptr_t a_start = (uintptr_t)a->data;
    uintptr_t a_end = a_start + a->length;
    uintptr_t b_start = (uintptr_t)b->data;
    uintptr_t b_end = b_start + b->length;
    
    return a_start < b_end && b_start < a_end;
}

intptr_t mp_memory_view_copy(mp_memory_view_t *dest, mp_memory_view_t *src) {
    if (!dest || !src || !dest->data || !src->data) {
        return -1;
    }
    
    // Check if destination has enough capacity
    if (dest->offset + dest->length + src->length > dest->capacity) {
        return -1;
    }
    
    // Check for overlap - if so, we need to handle it carefully
    if (mp_memory_view_overlaps(dest, src)) {
        // Overlapping regions - use memmove
        size_t copy_len = (src->length < dest->capacity - dest->offset - dest->length) ? 
                          src->length : dest->capacity - dest->offset - dest->length;
        memmove(dest->data + dest->length, src->data, copy_len);
        dest->length += copy_len;
        return copy_len;
    } else {
        // No overlap - use memcpy
        size_t copy_len = (src->length < dest->capacity - dest->offset - dest->length) ? 
                          src->length : dest->capacity - dest->offset - dest->length;
        memcpy(dest->data + dest->length, src->data, copy_len);
        dest->length += copy_len;
        return copy_len;
    }
}

int mp_memory_view_compare(mp_memory_view_t *a, mp_memory_view_t *b) {
    if (!a || !b) {
        return a ? 1 : (b ? -1 : 0);
    }
    
    size_t min_len = a->length < b->length ? a->length : b->length;
    int cmp = memcmp(a->data, b->data, min_len);
    
    if (cmp != 0) {
        return cmp;
    }
    
    // If equal up to min_len, the shorter one is "less"
    if (a->length < b->length) {
        return -1;
    } else if (a->length > b->length) {
        return 1;
    }
    
    return 0;
}

intptr_t mp_memory_view_find_byte(mp_memory_view_t *view, uint8_t byte) {
    if (!view || !view->data) {
        return -1;
    }
    
    for (size_t i = 0; i < view->length; i++) {
        if (view->data[i] == byte) {
            return (intptr_t)i;
        }
    }
    
    return -1;
}

intptr_t mp_memory_view_find_pattern(mp_memory_view_t *view, const uint8_t *pattern, size_t pattern_len) {
    if (!view || !view->data || !pattern || pattern_len == 0 || pattern_len > view->length) {
        return -1;
    }
    
    for (size_t i = 0; i <= view->length - pattern_len; i++) {
        if (memcmp(view->data + i, pattern, pattern_len) == 0) {
            return (intptr_t)i;
        }
    }
    
    return -1;
}
