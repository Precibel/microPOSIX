#include "microposix/mm/gc.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// Global GC instance
mp_gc_t *mp_gc_global = NULL;

// Helper to get GC object header from pointer
static inline mp_gc_object_t *get_gc_object(void *ptr) {
    if (!ptr) return NULL;
    return (mp_gc_object_t *)((uint8_t *)ptr - sizeof(mp_gc_object_t));
}

// Helper to get data pointer from GC object
static inline void *get_gc_data(mp_gc_object_t *obj) {
    return (void *)((uint8_t *)obj + sizeof(mp_gc_object_t));
}

// Helper to get object size including header
static inline size_t get_gc_total_size(mp_gc_object_t *obj) {
    return obj->size + sizeof(mp_gc_object_t);
}

// Mark function type
typedef void (*mp_gc_mark_fn_t)(mp_gc_t *gc, void *ptr);

// Mark an object
static void mark_object(mp_gc_t *gc, mp_gc_object_t *obj) {
    if (!obj || obj->flags & MP_GC_FLAG_MARKED) {
        return;
    }
    
    obj->flags |= MP_GC_FLAG_MARKED;
    gc->stats.objects_collected--; // Will be adjusted later
    
    // Push to mark stack
    if (gc->mark_stack_pos < gc->mark_stack_size) {
        gc->mark_stack[gc->mark_stack_pos++] = obj;
    }
}

// Process mark stack
static void process_mark_stack(mp_gc_t *gc) {
    while (gc->mark_stack_pos > 0) {
        mp_gc_object_t *obj = gc->mark_stack[--gc->mark_stack_pos];
        
        // Here we would normally trace references from the object
        // For now, we just mark it
        // In a full implementation, we would:
        // 1. Check if object has a custom tracer
        // 2. Scan object memory for pointers to other GC objects
        // 3. Mark those objects
        
        // For simplicity, we'll just mark the object itself
        // A real implementation would need type information
    }
}

// Sweep phase - free unmarked objects
static size_t sweep_phase(mp_gc_t *gc) {
    size_t collected = 0;
    size_t bytes_collected = 0;
    
    // Sweep young generation
    mp_gc_object_t *obj = gc->young_list;
    mp_gc_object_t *prev = NULL;
    
    while (obj) {
        mp_gc_object_t *next = obj->next;
        
        if (!(obj->flags & MP_GC_FLAG_MARKED)) {
            // Free this object
            if (obj->finalizer) {
                obj->finalizer(get_gc_data(obj));
            }
            
            // Remove from young list
            if (prev) {
                prev->next = next;
            } else {
                gc->young_list = next;
            }
            
            // Free memory
            free(obj);
            
            collected++;
            bytes_collected += get_gc_total_size(obj);
            gc->num_objects--;
        } else {
            // Clear mark flag
            obj->flags &= ~MP_GC_FLAG_MARKED;
            prev = obj;
        }
        
        obj = next;
    }
    
    // Sweep old generation
    obj = gc->old_list;
    prev = NULL;
    
    while (obj) {
        mp_gc_object_t *next = obj->next;
        
        if (!(obj->flags & MP_GC_FLAG_MARKED)) {
            // Free this object
            if (obj->finalizer) {
                obj->finalizer(get_gc_data(obj));
            }
            
            // Remove from old list
            if (prev) {
                prev->next = next;
            } else {
                gc->old_list = next;
            }
            
            // Free memory
            free(obj);
            
            collected++;
            bytes_collected += get_gc_total_size(obj);
            gc->num_objects--;
        } else {
            // Clear mark flag
            obj->flags &= ~MP_GC_FLAG_MARKED;
            prev = obj;
        }
        
        obj = next;
    }
    
    gc->stats.objects_collected = collected;
    gc->stats.bytes_collected = bytes_collected;
    gc->stats.bytes_freed += bytes_collected;
    
    return collected;
}

// Mark phase - mark all reachable objects
static void mark_phase(mp_gc_t *gc) {
    // Mark all roots
    mp_gc_object_t *root = gc->root_list;
    while (root) {
        mark_object(gc, root);
        root = root->next;
    }
    
    // Process mark stack
    process_mark_stack(gc);
}

// Promote young generation objects to old generation
static void promote_young(mp_gc_t *gc) {
    mp_gc_object_t *obj = gc->young_list;
    mp_gc_object_t *prev = NULL;
    
    while (obj) {
        mp_gc_object_t *next = obj->next;
        
        // Promote to old generation
        obj->flags &= ~MP_GC_FLAG_YOUNG;
        obj->flags |= MP_GC_FLAG_OLD;
        obj->generation = gc->generation_counter;
        
        // Move to old list
        if (prev) {
            prev->next = next;
        } else {
            gc->young_list = next;
        }
        
        // Add to old list
        obj->next = gc->old_list;
        if (gc->old_list) {
            gc->old_list->prev = obj;
        }
        gc->old_list = obj;
        obj->prev = NULL;
        
        obj = next;
    }
}

int mp_gc_init(mp_gc_t *gc, mp_gc_config_t *config) {
    if (!gc || !config) {
        return -1;
    }
    
    // Copy configuration
    memcpy(&gc->config, config, sizeof(mp_gc_config_t));
    
    // Initialize state
    gc->heap = config->heap;
    gc->objects = NULL;
    gc->num_objects = 0;
    gc->max_objects = config->max_objects;
    gc->root_list = NULL;
    gc->young_list = NULL;
    gc->old_list = NULL;
    gc->object_map = NULL;
    gc->object_map_size = 0;
    gc->generation_counter = 0;
    gc->last_gc_time = 0;
    gc->gc_running = false;
    gc->gc_enabled = true;
    gc->mark_stack = NULL;
    gc->mark_stack_size = 0;
    gc->mark_stack_pos = 0;
    gc->ref_count_mutex = NULL;
    
    // Initialize statistics
    memset(&gc->stats, 0, sizeof(gc->stats));
    gc->stats.total_objects = 0;
    gc->stats.live_objects = 0;
    
    // Allocate object array
    if (config->max_objects > 0) {
        gc->objects = (mp_gc_object_t **)malloc(config->max_objects * sizeof(mp_gc_object_t *));
        if (!gc->objects) {
            return -1;
        }
        memset(gc->objects, 0, config->max_objects * sizeof(mp_gc_object_t *));
    }
    
    // Allocate mark stack
    gc->mark_stack_size = config->max_objects > 100 ? config->max_objects / 10 : 100;
    gc->mark_stack = (mp_gc_object_t **)malloc(gc->mark_stack_size * sizeof(mp_gc_object_t *));
    if (!gc->mark_stack) {
        free(gc->objects);
        gc->objects = NULL;
        return -1;
    }
    
    // Create ref count mutex if using ref counting (placeholder for now)
    if (config->use_ref_counting) {
        gc->ref_count_mutex = NULL; // Would be mp_mutex_create() in real implementation
    }
    
    return 0;
}

void mp_gc_destroy(mp_gc_t *gc) {
    if (!gc) {
        return;
    }
    
    // Free all objects
    mp_gc_collect_full(gc);
    
    // Free resources
    if (gc->objects) {
        free(gc->objects);
        gc->objects = NULL;
    }
    
    if (gc->mark_stack) {
        free(gc->mark_stack);
        gc->mark_stack = NULL;
    }
    
    if (gc->object_map) {
        free(gc->object_map);
        gc->object_map = NULL;
    }
    
    if (gc->ref_count_mutex) {
        // mp_mutex_destroy(gc->ref_count_mutex);
        gc->ref_count_mutex = NULL;
    }
    
    gc->num_objects = 0;
    gc->max_objects = 0;
}

void *mp_gc_alloc(mp_gc_t *gc, size_t size) {
    return mp_gc_alloc_with_finalizer(gc, size, NULL, NULL);
}

void *mp_gc_alloc_with_finalizer(mp_gc_t *gc, size_t size, void (*finalizer)(void *), void *finalizer_data) {
    if (!gc || size == 0) {
        return NULL;
    }
    
    // Check if we need to run GC
    if (gc->gc_enabled && gc->num_objects >= gc->max_objects) {
        mp_gc_collect(gc);
    }
    
    // Allocate object header + data
    size_t total_size = sizeof(mp_gc_object_t) + size;
    mp_gc_object_t *obj = (mp_gc_object_t *)malloc(total_size);
    
    if (!obj) {
        // Try GC and retry
        if (gc->gc_enabled) {
            mp_gc_collect_full(gc);
            obj = (mp_gc_object_t *)malloc(total_size);
        }
        if (!obj) {
            return NULL;
        }
    }
    
    // Initialize object
    obj->gc = gc;
    obj->ref_count = 1; // Start with ref count of 1
    obj->flags = MP_GC_FLAG_YOUNG;
    obj->size = size;
    obj->generation = gc->generation_counter;
    obj->timestamp = 0; // TODO: Add timestamp
    obj->finalizer = finalizer;
    obj->finalizer_data = finalizer_data;
    
    // Add to object list
    obj->next = gc->young_list;
    obj->prev = NULL;
    if (gc->young_list) {
        gc->young_list->prev = obj;
    }
    gc->young_list = obj;
    
    // Update statistics
    gc->num_objects++;
    gc->stats.total_objects++;
    gc->stats.live_objects++;
    gc->stats.bytes_allocated += total_size;
    
    // Store in objects array if we have space
    if (gc->objects && gc->num_objects <= gc->max_objects) {
        gc->objects[gc->num_objects - 1] = obj;
    }
    
    return get_gc_data(obj);
}

void mp_gc_free(mp_gc_t *gc, void *ptr) {
    if (!gc || !ptr) {
        return;
    }
    
    mp_gc_object_t *obj = get_gc_object(ptr);
    
    if (!obj || obj->gc != gc) {
        return; // Not a GC object or wrong GC
    }
    
    // Call finalizer if present
    if (obj->finalizer) {
        obj->finalizer(ptr);
    }
    
    // Remove from appropriate list
    if (obj->flags & MP_GC_FLAG_YOUNG) {
        if (obj->prev) {
            obj->prev->next = obj->next;
        } else {
            gc->young_list = obj->next;
        }
        if (obj->next) {
            obj->next->prev = obj->prev;
        }
    } else {
        if (obj->prev) {
            obj->prev->next = obj->next;
        } else {
            gc->old_list = obj->next;
        }
        if (obj->next) {
            obj->next->prev = obj->prev;
        }
    }
    
    // Update statistics
    gc->num_objects--;
    gc->stats.live_objects--;
    gc->stats.bytes_freed += get_gc_total_size(obj);
    
    // Free memory
    free(obj);
}

void mp_gc_incref(mp_gc_t *gc, void *ptr) {
    if (!gc || !ptr) {
        return;
    }
    
    mp_gc_object_t *obj = get_gc_object(ptr);
    
    if (!obj || obj->gc != gc) {
        return;
    }
    
    if (gc->ref_count_mutex) {
        // mp_mutex_lock(gc->ref_count_mutex);
    }
    
    obj->ref_count++;
    
    if (gc->ref_count_mutex) {
        // mp_mutex_unlock(gc->ref_count_mutex);
    }
}

void mp_gc_decref(mp_gc_t *gc, void *ptr) {
    if (!gc || !ptr) {
        return;
    }
    
    mp_gc_object_t *obj = get_gc_object(ptr);
    
    if (!obj || obj->gc != gc) {
        return;
    }
    
    bool should_free = false;
    
    if (gc->ref_count_mutex) {
        // mp_mutex_lock(gc->ref_count_mutex);
    }
    
    if (obj->ref_count > 0) {
        obj->ref_count--;
        if (obj->ref_count == 0) {
            should_free = true;
        }
    }
    
    if (gc->ref_count_mutex) {
        // mp_mutex_unlock(gc->ref_count_mutex);
    }
    
    if (should_free) {
        mp_gc_free(gc, ptr);
    }
}

uint32_t mp_gc_get_refcount(mp_gc_t *gc, void *ptr) {
    if (!gc || !ptr) {
        return 0;
    }
    
    mp_gc_object_t *obj = get_gc_object(ptr);
    
    if (!obj || obj->gc != gc) {
        return 0;
    }
    
    return obj->ref_count;
}

int mp_gc_add_root(mp_gc_t *gc, void *ptr, mp_gc_root_type_t type, const char *name) {
    (void)type;
    (void)name;
    
    if (!gc || !ptr) {
        return -1;
    }
    
    mp_gc_object_t *obj = get_gc_object(ptr);
    
    if (!obj || obj->gc != gc) {
        return -1; // Not a GC object
    }
    
    // Mark as root
    obj->flags |= MP_GC_FLAG_ROOT;
    
    // Add to root list
    obj->next = gc->root_list;
    if (gc->root_list) {
        gc->root_list->prev = obj;
    }
    gc->root_list = obj;
    obj->prev = NULL;
    
    return 0;
}

void mp_gc_remove_root(mp_gc_t *gc, void *ptr) {
    if (!gc || !ptr) {
        return;
    }
    
    mp_gc_object_t *obj = get_gc_object(ptr);
    
    if (!obj || obj->gc != gc) {
        return;
    }
    
    // Clear root flag
    obj->flags &= ~MP_GC_FLAG_ROOT;
    
    // Remove from root list
    if (obj->prev) {
        obj->prev->next = obj->next;
    } else {
        gc->root_list = obj->next;
    }
    if (obj->next) {
        obj->next->prev = obj->prev;
    }
    
    obj->prev = NULL;
    obj->next = NULL;
}

size_t mp_gc_collect(mp_gc_t *gc) {
    if (!gc || !gc->gc_enabled || gc->gc_running) {
        return 0;
    }
    
    gc->gc_running = true;
    
    // If using generational GC, collect young generation
    if (gc->config.use_generational) {
        size_t collected = mp_gc_collect_young(gc);
        gc->gc_running = false;
        return collected;
    }
    
    // Otherwise, do full collection
    size_t collected = mp_gc_collect_full(gc);
    gc->gc_running = false;
    return collected;
}

size_t mp_gc_collect_full(mp_gc_t *gc) {
    if (!gc || !gc->gc_enabled || gc->gc_running) {
        return 0;
    }
    
    gc->gc_running = true;
    gc->stats.gc_runs++;
    
    // Mark phase
    mark_phase(gc);
    
    // Sweep phase
    size_t collected = sweep_phase(gc);
    
    // Update generation counter
    gc->generation_counter++;
    
    gc->gc_running = false;
    gc->stats.live_objects = gc->num_objects;
    
    return collected;
}

size_t mp_gc_collect_young(mp_gc_t *gc) {
    if (!gc || !gc->gc_enabled || gc->gc_running) {
        return 0;
    }
    
    gc->gc_running = true;
    gc->stats.gc_runs++;
    
    // Mark phase for young generation
    // Mark all roots
    mp_gc_object_t *root = gc->root_list;
    while (root) {
        // If root is in young generation, mark it
        if (root->flags & MP_GC_FLAG_YOUNG) {
            mark_object(gc, root);
        }
        root = root->next;
    }
    
    // Process mark stack
    process_mark_stack(gc);
    
    // Sweep young generation
    size_t collected = 0;
    size_t bytes_collected = 0;
    
    mp_gc_object_t *obj = gc->young_list;
    mp_gc_object_t *prev = NULL;
    
    while (obj) {
        mp_gc_object_t *next = obj->next;
        
        if (!(obj->flags & MP_GC_FLAG_MARKED)) {
            // Free this object
            if (obj->finalizer) {
                obj->finalizer(get_gc_data(obj));
            }
            
            // Remove from young list
            if (prev) {
                prev->next = next;
            } else {
                gc->young_list = next;
            }
            
            // Free memory
            free(obj);
            
            collected++;
            bytes_collected += get_gc_total_size(obj);
            gc->num_objects--;
        } else {
            // Clear mark flag
            obj->flags &= ~MP_GC_FLAG_MARKED;
            prev = obj;
        }
        
        obj = next;
    }
    
    // Promote surviving young objects to old generation
    promote_young(gc);
    
    // Update statistics
    gc->stats.objects_collected = collected;
    gc->stats.bytes_collected = bytes_collected;
    gc->stats.bytes_freed += bytes_collected;
    gc->stats.live_objects = gc->num_objects;
    
    gc->gc_running = false;
    
    return collected;
}

void mp_gc_set_auto(mp_gc_t *gc, bool enabled) {
    if (gc) {
        gc->gc_enabled = enabled;
    }
}

bool mp_gc_is_enabled(mp_gc_t *gc) {
    if (!gc) {
        return false;
    }
    return gc->gc_enabled;
}

void mp_gc_get_stats(mp_gc_t *gc, mp_gc_stats_t *stats) {
    if (!gc || !stats) {
        return;
    }
    memcpy(stats, &gc->stats, sizeof(*stats));
}

void mp_gc_reset_stats(mp_gc_t *gc) {
    if (!gc) {
        return;
    }
    memset(&gc->stats, 0, sizeof(gc->stats));
}

bool mp_gc_is_gc_object(mp_gc_t *gc, void *ptr) {
    if (!gc || !ptr) {
        return false;
    }
    
    mp_gc_object_t *obj = get_gc_object(ptr);
    
    if (!obj) {
        return false;
    }
    
    // Check if object belongs to this GC
    return obj->gc == gc;
}

size_t mp_gc_get_size(mp_gc_t *gc, void *ptr) {
    if (!gc || !ptr) {
        return 0;
    }
    
    mp_gc_object_t *obj = get_gc_object(ptr);
    
    if (!obj || obj->gc != gc) {
        return 0;
    }
    
    return obj->size;
}

void mp_gc_pin(mp_gc_t *gc, void *ptr) {
    if (!gc || !ptr) {
        return;
    }
    
    mp_gc_object_t *obj = get_gc_object(ptr);
    
    if (!obj || obj->gc != gc) {
        return;
    }
    
    obj->flags |= MP_GC_FLAG_PINNED;
}

void mp_gc_unpin(mp_gc_t *gc, void *ptr) {
    if (!gc || !ptr) {
        return;
    }
    
    mp_gc_object_t *obj = get_gc_object(ptr);
    
    if (!obj || obj->gc != gc) {
        return;
    }
    
    obj->flags &= ~MP_GC_FLAG_PINNED;
}

bool mp_gc_is_pinned(mp_gc_t *gc, void *ptr) {
    if (!gc || !ptr) {
        return false;
    }
    
    mp_gc_object_t *obj = get_gc_object(ptr);
    
    if (!obj || obj->gc != gc) {
        return false;
    }
    
    return (obj->flags & MP_GC_FLAG_PINNED) != 0;
}

void mp_gc_register_tracer(mp_gc_t *gc, const char *type_name, mp_gc_trace_fn_t trace_fn) {
    (void)gc;
    (void)type_name;
    (void)trace_fn;
    // TODO: Implement custom tracers
}

void mp_gc_dump(mp_gc_t *gc, mp_heap_dump_callback_t callback, void *user_data) {
    if (!gc || !callback) {
        return;
    }
    
    char line[256];
    mp_gc_stats_t stats;
    
    mp_gc_get_stats(gc, &stats);
    
    snprintf(line, sizeof(line), "GC Dump:");
    callback(line, user_data);
    
    snprintf(line, sizeof(line), "  Total Objects: %zu", stats.total_objects);
    callback(line, user_data);
    
    snprintf(line, sizeof(line), "  Live Objects: %zu", stats.live_objects);
    callback(line, user_data);
    
    snprintf(line, sizeof(line), "  Bytes Allocated: %zu", stats.bytes_allocated);
    callback(line, user_data);
    
    snprintf(line, sizeof(line), "  Bytes Freed: %zu", stats.bytes_freed);
    callback(line, user_data);
    
    snprintf(line, sizeof(line), "  GC Runs: %zu", stats.gc_runs);
    callback(line, user_data);
    
    snprintf(line, sizeof(line), "  Objects Collected (last): %zu", stats.objects_collected);
    callback(line, user_data);
    
    snprintf(line, sizeof(line), "  Bytes Collected (last): %zu", stats.bytes_collected);
    callback(line, user_data);
    
    // Dump root list
    snprintf(line, sizeof(line), "  Roots:");
    callback(line, user_data);
    
    mp_gc_object_t *root = gc->root_list;
    int count = 0;
    while (root) {
        snprintf(line, sizeof(line), "    Root %d: %p (refcount: %u)", 
                 count++, get_gc_data(root), root->ref_count);
        callback(line, user_data);
        root = root->next;
    }
    
    // Dump young generation
    snprintf(line, sizeof(line), "  Young Generation (%zu objects):", gc->num_objects);
    callback(line, user_data);
    
    mp_gc_object_t *obj = gc->young_list;
    count = 0;
    while (obj) {
        snprintf(line, sizeof(line), "    Object %d: %p, size: %zu, refcount: %u",
                 count++, get_gc_data(obj), obj->size, obj->ref_count);
        callback(line, user_data);
        obj = obj->next;
    }
    
    // Dump old generation
    snprintf(line, sizeof(line), "  Old Generation:");
    callback(line, user_data);
    
    obj = gc->old_list;
    count = 0;
    while (obj) {
        snprintf(line, sizeof(line), "    Object %d: %p, size: %zu, refcount: %u, generation: %u",
                 count++, get_gc_data(obj), obj->size, obj->ref_count, obj->generation);
        callback(line, user_data);
        obj = obj->next;
    }
}

bool mp_gc_validate(mp_gc_t *gc) {
    if (!gc) {
        return false;
    }
    
    // Check basic state
    if (gc->gc_running) {
        return false; // GC is running
    }
    
    // Count objects in all lists
    size_t total_counted = 0;
    
    mp_gc_object_t *obj = gc->young_list;
    while (obj) {
        total_counted++;
        obj = obj->next;
    }
    
    obj = gc->old_list;
    while (obj) {
        total_counted++;
        obj = obj->next;
    }
    
    obj = gc->root_list;
    while (obj) {
        total_counted++;
        obj = obj->next;
    }
    
    // Check if counted objects match
    if (total_counted != gc->num_objects) {
        return false;
    }
    
    return true;
}

// Global GC functions
int mp_gc_global_init(mp_shared_heap_t *heap, size_t max_objects) {
    if (mp_gc_global) {
        return -1; // Already initialized
    }
    
    mp_gc_global = (mp_gc_t *)malloc(sizeof(mp_gc_t));
    if (!mp_gc_global) {
        return -1;
    }
    
    mp_gc_config_t config = {
        .heap = heap,
        .initial_size = 1024 * 1024, // 1MB
        .max_objects = max_objects,
        .use_ref_counting = true,
        .use_mark_sweep = true,
        .use_generational = false,
        .young_gen_size = 256 * 1024, // 256KB
        .threshold = 100,
        .gc_interval = 1000 // 1 second
    };
    
    return mp_gc_init(mp_gc_global, &config);
}

void *mp_gc_global_alloc(size_t size) {
    if (!mp_gc_global) {
        return NULL;
    }
    return mp_gc_alloc(mp_gc_global, size);
}

void mp_gc_global_free(void *ptr) {
    if (!mp_gc_global) {
        return;
    }
    mp_gc_free(mp_gc_global, ptr);
}

void mp_gc_global_incref(void *ptr) {
    if (!mp_gc_global) {
        return;
    }
    mp_gc_incref(mp_gc_global, ptr);
}

void mp_gc_global_decref(void *ptr) {
    if (!mp_gc_global) {
        return;
    }
    mp_gc_decref(mp_gc_global, ptr);
}

size_t mp_gc_global_collect(void) {
    if (!mp_gc_global) {
        return 0;
    }
    return mp_gc_collect(mp_gc_global);
}

void mp_gc_global_get_stats(mp_gc_stats_t *stats) {
    if (!mp_gc_global) {
        if (stats) {
            memset(stats, 0, sizeof(*stats));
        }
        return;
    }
    mp_gc_get_stats(mp_gc_global, stats);
}

int mp_gc_global_add_root(void *ptr, mp_gc_root_type_t type, const char *name) {
    if (!mp_gc_global) {
        return -1;
    }
    return mp_gc_add_root(mp_gc_global, ptr, type, name);
}

void mp_gc_global_remove_root(void *ptr) {
    if (!mp_gc_global) {
        return;
    }
    mp_gc_remove_root(mp_gc_global, ptr);
}
