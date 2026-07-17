#ifndef MICROPOSIX_GC_H
#define MICROPOSIX_GC_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "microposix/mm/shared_heap.h"

/**
 * @file gc.h
 * @brief Garbage collection system for microPOSIX
 * 
 * Provides multiple garbage collection strategies:
 * - Reference counting (simple, deterministic)
 * - Mark-and-sweep (handles cycles)
 * - Generational GC (optimized for embedded)
 * 
 * Designed for memory-constrained embedded systems with
 * minimal overhead and predictable behavior.
 */

// Forward declarations
typedef struct mp_gc mp_gc_t;
typedef struct mp_gc_object mp_gc_object_t;

/**
 * @brief GC configuration
 */
typedef struct {
    mp_shared_heap_t *heap;        ///< Heap to manage with GC
    size_t initial_size;           ///< Initial heap size for GC-managed objects
    size_t max_objects;            ///< Maximum number of tracked objects
    bool use_ref_counting;         ///< Enable reference counting
    bool use_mark_sweep;           ///< Enable mark-and-sweep
    bool use_generational;         ///< Enable generational GC
    size_t young_gen_size;         ///< Size of young generation (if generational)
    size_t threshold;              ///< Allocation threshold to trigger GC
    uint32_t gc_interval;          ///< Time between automatic GC runs (ms)
} mp_gc_config_t;

/**
 * @brief GC statistics
 */
typedef struct {
    size_t total_objects;          ///< Total number of tracked objects
    size_t live_objects;           ///< Number of live objects
    size_t dead_objects;           ///< Number of dead objects
    size_t bytes_allocated;        ///< Total bytes allocated
    size_t bytes_freed;            ///< Total bytes freed by GC
    size_t gc_runs;                ///< Number of GC runs
    size_t gc_time;                ///< Total time spent in GC (us)
    size_t max_pause_time;         ///< Maximum GC pause time (us)
    size_t last_gc_time;           ///< Last GC run time (us)
    size_t objects_collected;      ///< Objects collected in last run
    size_t bytes_collected;        ///< Bytes collected in last run
} mp_gc_stats_t;

/**
 * @brief GC object header (prepended to all GC-managed objects)
 */
struct mp_gc_object {
    mp_gc_object_t *next;          ///< Next object in list
    mp_gc_object_t *prev;          ///< Previous object in list
    mp_gc_t *gc;                   ///< Owning GC instance
    uint32_t ref_count;            ///< Reference count
    uint32_t flags;                ///< Object flags
    size_t size;                   ///< Size of the object (excluding header)
    uint32_t generation;           ///< Generation number (for generational GC)
    uint32_t timestamp;            ///< Allocation timestamp
    void (*finalizer)(void *);     ///< Finalizer function
    void *finalizer_data;         ///< Data for finalizer
};

/**
 * @brief GC object flags
 */
#define MP_GC_FLAG_MARKED       (1 << 0)    ///< Object is marked (for mark-and-sweep)
#define MP_GC_FLAG_ROOT         (1 << 1)    ///< Object is a root
#define MP_GC_FLAG_YOUNG        (1 << 2)    ///< Object is in young generation
#define MP_GC_FLAG_OLD          (1 << 3)    ///< Object is in old generation
#define MP_GC_FLAG_FINALIZER    (1 << 4)    ///< Object has a finalizer
#define MP_GC_FLAG_PINNED       (1 << 5)    ///< Object is pinned (won't be moved)

/**
 * @brief GC structure
 */
struct mp_gc {
    mp_shared_heap_t *heap;        ///< Underlying heap
    mp_gc_config_t config;         ///< GC configuration
    mp_gc_stats_t stats;           ///< GC statistics
    
    mp_gc_object_t **objects;       ///< Array of tracked objects (for fast lookup)
    size_t num_objects;           ///< Current number of tracked objects
    size_t max_objects;           ///< Maximum number of tracked objects
    
    mp_gc_object_t *root_list;    ///< List of root objects
    mp_gc_object_t *young_list;   ///< List of young generation objects
    mp_gc_object_t *old_list;     ///< List of old generation objects
    
    mp_gc_object_t **object_map;  ///< Map from pointer to object header
    size_t object_map_size;       ///< Size of object map
    
    uint32_t generation_counter;   ///< Current generation counter
    uint32_t last_gc_time;         ///< Timestamp of last GC run
    bool gc_running;              ///< Whether GC is currently running
    bool gc_enabled;              ///< Whether GC is enabled
    
    // For mark-and-sweep
    mp_gc_object_t **mark_stack;   ///< Stack for mark phase
    size_t mark_stack_size;       ///< Size of mark stack
    size_t mark_stack_pos;        ///< Current position in mark stack
    
    // For reference counting
    mp_mutex_t *ref_count_mutex;  ///< Mutex for ref count operations
};

/**
 * @brief GC root type
 */
typedef enum {
    MP_GC_ROOT_GLOBAL,            ///< Global variable
    MP_GC_ROOT_STACK,             ///< Stack variable
    MP_GC_ROOT_STATIC,            ///< Static variable
    MP_GC_ROOT_THREAD_LOCAL,      ///< Thread-local variable
    MP_GC_ROOT_REGISTER,          ///< Register (temporary)
} mp_gc_root_type_t;

/**
 * @brief GC root information
 */
typedef struct {
    void *ptr;                    ///< Pointer to the root
    mp_gc_root_type_t type;       ///< Type of root
    const char *name;             ///< Name of the root (for debugging)
    mp_thread_t *thread;         ///< Owning thread (if thread-local)
} mp_gc_root_t;

/**
 * @brief Initialize a garbage collector
 * 
 * @param gc The GC instance to initialize
 * @param config Configuration for the GC
 * @return 0 on success, -1 on error
 */
int mp_gc_init(mp_gc_t *gc, mp_gc_config_t *config);

/**
 * @brief Destroy a garbage collector
 * 
 * @param gc The GC instance to destroy
 */
void mp_gc_destroy(mp_gc_t *gc);

/**
 * @brief Allocate a GC-managed object
 * 
 * @param gc The GC instance
 * @param size Size of the object (excluding header)
 * @return Pointer to the object, or NULL on failure
 */
void *mp_gc_alloc(mp_gc_t *gc, size_t size);

/**
 * @brief Allocate a GC-managed object with a finalizer
 * 
 * @param gc The GC instance
 * @param size Size of the object
 * @param finalizer Finalizer function to call when object is collected
 * @param finalizer_data Data to pass to finalizer
 * @return Pointer to the object, or NULL on failure
 */
void *mp_gc_alloc_with_finalizer(mp_gc_t *gc, size_t size, void (*finalizer)(void *), void *finalizer_data);

/**
 * @brief Free a GC-managed object (explicit free)
 * 
 * @param gc The GC instance
 * @param ptr Pointer to the object
 */
void mp_gc_free(mp_gc_t *gc, void *ptr);

/**
 * @brief Increment reference count for an object
 * 
 * @param gc The GC instance
 * @param ptr Pointer to the object
 */
void mp_gc_incref(mp_gc_t *gc, void *ptr);

/**
 * @brief Decrement reference count for an object
 * 
 * If reference count reaches zero, the object is freed.
 * 
 * @param gc The GC instance
 * @param ptr Pointer to the object
 */
void mp_gc_decref(mp_gc_t *gc, void *ptr);

/**
 * @brief Get reference count for an object
 * 
 * @param gc The GC instance
 * @param ptr Pointer to the object
 * @return Reference count, or 0 if not a GC object
 */
uint32_t mp_gc_get_refcount(mp_gc_t *gc, void *ptr);

/**
 * @brief Add a root to the GC
 * 
 * Roots are objects that are always considered live.
 * 
 * @param gc The GC instance
 * @param ptr Pointer to the root object
 * @param type Type of root
 * @param name Name for debugging
 * @return 0 on success, -1 on error
 */
int mp_gc_add_root(mp_gc_t *gc, void *ptr, mp_gc_root_type_t type, const char *name);

/**
 * @brief Remove a root from the GC
 * 
 * @param gc The GC instance
 * @param ptr Pointer to the root object
 */
void mp_gc_remove_root(mp_gc_t *gc, void *ptr);

/**
 * @brief Run garbage collection
 * 
 * @param gc The GC instance
 * @return Number of objects collected
 */
size_t mp_gc_collect(mp_gc_t *gc);

/**
 * @brief Run a full GC cycle (mark-and-sweep)
 * 
 * @param gc The GC instance
 * @return Number of objects collected
 */
size_t mp_gc_collect_full(mp_gc_t *gc);

/**
 * @brief Run a young generation GC (if generational)
 * 
 * @param gc The GC instance
 * @return Number of objects collected
 */
size_t mp_gc_collect_young(mp_gc_t *gc);

/**
 * @brief Enable automatic GC
 * 
 * @param gc The GC instance
 * @param enabled Whether to enable automatic GC
 */
void mp_gc_set_auto(mp_gc_t *gc, bool enabled);

/**
 * @brief Check if GC is enabled
 * 
 * @param gc The GC instance
 * @return true if GC is enabled
 */
bool mp_gc_is_enabled(mp_gc_t *gc);

/**
 * @brief Get GC statistics
 * 
 * @param gc The GC instance
 * @param stats Output structure for statistics
 */
void mp_gc_get_stats(mp_gc_t *gc, mp_gc_stats_t *stats);

/**
 * @brief Reset GC statistics
 * 
 * @param gc The GC instance
 */
void mp_gc_reset_stats(mp_gc_t *gc);

/**
 * @brief Check if a pointer is a GC-managed object
 * 
 * @param gc The GC instance
 * @param ptr Pointer to check
 * @return true if the pointer is a GC-managed object
 */
bool mp_gc_is_gc_object(mp_gc_t *gc, void *ptr);

/**
 * @brief Get the size of a GC-managed object
 * 
 * @param gc The GC instance
 * @param ptr Pointer to the object
 * @return Size of the object, or 0 if not a GC object
 */
size_t mp_gc_get_size(mp_gc_t *gc, void *ptr);

/**
 * @brief Pin an object (prevent it from being moved by GC)
 * 
 * @param gc The GC instance
 * @param ptr Pointer to the object
 */
void mp_gc_pin(mp_gc_t *gc, void *ptr);

/**
 * @brief Unpin an object
 * 
 * @param gc The GC instance
 * @param ptr Pointer to the object
 */
void mp_gc_unpin(mp_gc_t *gc, void *ptr);

/**
 * @brief Check if an object is pinned
 * 
 * @param gc The GC instance
 * @param ptr Pointer to the object
 * @return true if the object is pinned
 */
bool mp_gc_is_pinned(mp_gc_t *gc, void *ptr);

/**
 * @brief Register a custom object tracer
 * 
 * Custom tracers are called during mark phase to trace
 * references from custom types.
 * 
 * @param gc The GC instance
 * @param type_name Name of the type to trace
 * @param trace_fn Function to trace references in the object
 */
typedef void (*mp_gc_trace_fn_t)(mp_gc_t *gc, void *obj, void (*mark_fn)(mp_gc_t *, void *));
void mp_gc_register_tracer(mp_gc_t *gc, const char *type_name, mp_gc_trace_fn_t trace_fn);

/**
 * @brief Dump GC information
 * 
 * @param gc The GC instance
 * @param callback Callback to receive output lines
 * @param user_data User data for callback
 */
void mp_gc_dump(mp_gc_t *gc, mp_heap_dump_callback_t callback, void *user_data);

/**
 * @brief Validate GC integrity
 * 
 * @param gc The GC instance
 * @return true if GC is in a valid state
 */
bool mp_gc_validate(mp_gc_t *gc);

/**
 * @brief Global GC instance
 */
extern mp_gc_t *mp_gc_global;

/**
 * @brief Initialize the global GC
 * 
 * @param heap Heap to use for GC
 * @param max_objects Maximum number of objects to track
 * @return 0 on success, -1 on error
 */
int mp_gc_global_init(mp_shared_heap_t *heap, size_t max_objects);

/**
 * @brief Allocate from global GC
 * 
 * @param size Size of object
 * @return Pointer to object
 */
void *mp_gc_global_alloc(size_t size);

/**
 * @brief Free from global GC
 * 
 * @param ptr Pointer to free
 */
void mp_gc_global_free(void *ptr);

/**
 * @brief Increment reference count (global GC)
 * 
 * @param ptr Pointer to object
 */
void mp_gc_global_incref(void *ptr);

/**
 * @brief Decrement reference count (global GC)
 * 
 * @param ptr Pointer to object
 */
void mp_gc_global_decref(void *ptr);

/**
 * @brief Run GC on global collector
 * 
 * @return Number of objects collected
 */
size_t mp_gc_global_collect(void);

/**
 * @brief Get global GC statistics
 * 
 * @param stats Output structure
 */
void mp_gc_global_get_stats(mp_gc_stats_t *stats);

/**
 * @brief Add a global root
 * 
 * @param ptr Pointer to root
 * @param type Root type
 * @param name Name for debugging
 * @return 0 on success, -1 on error
 */
int mp_gc_global_add_root(void *ptr, mp_gc_root_type_t type, const char *name);

/**
 * @brief Remove a global root
 * 
 * @param ptr Pointer to root
 */
void mp_gc_global_remove_root(void *ptr);

/**
 * @brief Macro for allocating GC object with type
 */
#define mp_gc_alloc_type(gc, type) ((type *)mp_gc_alloc(gc, sizeof(type)))

/**
 * @brief Macro for allocating GC object with type (global)
 */
#define mp_gc_global_alloc_type(type) ((type *)mp_gc_global_alloc(sizeof(type)))

/**
 * @brief Macro for declaring a GC root (global variable)
 */
#define MP_GC_ROOT(type, name) type *name = NULL

/**
 * @brief Macro for initializing a GC root
 */
#define MP_GC_ROOT_INIT(gc, name, value) \
    do { \
        name = (typeof(name))(value); \
        if (gc) mp_gc_add_root(gc, (void *)name, MP_GC_ROOT_GLOBAL, #name); \
    } while (0)

/**
 * @brief Macro for declaring a GC root (stack variable)
 */
#define MP_GC_STACK_ROOT(type, name) type *name = NULL

/**
 * @brief Macro for pushing a stack root
 */
#define MP_GC_STACK_PUSH(gc, name) \
    do { \
        if (gc) mp_gc_add_root(gc, (void *)name, MP_GC_ROOT_STACK, #name); \
    } while (0)

/**
 * @brief Macro for popping a stack root
 */
#define MP_GC_STACK_POP(gc, name) \
    do { \
        if (gc) mp_gc_remove_root(gc, (void *)name); \
    } while (0)

#endif // MICROPOSIX_GC_H
