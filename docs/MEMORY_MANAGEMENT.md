# Memory Management in microPOSIX

This document describes the memory management features available in microPOSIX, including zero-copy memory views, shared heap management, garbage collection, and serialization support.

## Table of Contents

1. [Overview](#overview)
2. [Zero-Copy Memory Views](#zero-copy-memory-views)
3. [Shared Heap Memory](#shared-heap-memory)
4. [Garbage Collection](#garbage-collection)
5. [Memory Pools and Arenas](#memory-pools-and-arenas)
6. [Serialization Support](#serialization-support)
7. [Usage Examples](#usage-examples)
8. [Performance Considerations](#performance-considerations)
9. [API Reference](#api-reference)

---

## Overview

microPOSIX provides a comprehensive memory management subsystem designed for embedded systems with limited resources. The system includes:

- **Zero-copy memory views**: Efficient memory slicing without data copying
- **Shared heap management**: Thread-safe memory allocation with tracking
- **Garbage collection**: Reference counting and mark-and-sweep GC
- **Memory pools**: Fixed-size block allocation for real-time systems
- **Memory arenas**: Bulk allocation with zero-copy slicing
- **Serialization**: JSON, Protocol Buffers, and EDF support

All features are designed to work within the constraints of microcontroller environments, with minimal overhead and predictable behavior.

---

## Zero-Copy Memory Views

Memory views provide a way to reference portions of memory buffers without copying the underlying data. This is particularly useful for:

- Protocol parsing (HTTP, BLE, etc.)
- Buffer management
- Efficient data processing pipelines
- Memory-mapped I/O

### Key Features

- **Memory View (`mp_memory_view_t`)**: A view into a buffer with length, capacity, and offset
- **Memory Slice (`mp_memory_slice_t`)**: A typed view with element size and count
- **Memory Arena (`mp_memory_arena_t`)**: A bulk allocator for zero-copy operations

### Example Usage

```c
#include "microposix/mm/memory_view.h"

// Create a buffer
uint8_t buffer[1024];

// Create a memory view
mp_memory_view_t view = mp_memory_view_from_buffer(buffer, 1024);

// Create a slice (zero-copy)
mp_memory_view_t slice = mp_memory_view_slice(&view, 100, 500);

// Create a typed slice
mp_memory_slice_t int_slice = mp_memory_view_as_slice(&view, sizeof(int), 256);

// Allocate from arena
mp_memory_arena_t arena;
mp_memory_arena_init(&arena, buffer, 1024, 8);
void *ptr = mp_memory_arena_alloc(&arena, 100, 8);

// Create a view from arena allocation
mp_memory_view_t arena_view = mp_memory_arena_alloc_view(&arena, 100, 8);
```

### Memory View Operations

- `mp_memory_view_slice()`: Create a sub-view of a memory view
- `mp_memory_view_as_slice()`: Convert a view to a typed slice
- `mp_memory_view_copy()`: Copy data between views (handles overlap)
- `mp_memory_view_compare()`: Compare two views
- `mp_memory_view_find_byte()`: Find a byte in a view
- `mp_memory_view_find_pattern()`: Find a pattern in a view
- `mp_memory_view_overlaps()`: Check if two views overlap

### Memory Arena Operations

- `mp_memory_arena_alloc()`: Allocate memory from arena
- `mp_memory_arena_zalloc()`: Allocate and zero memory
- `mp_memory_arena_reset()`: Free all allocations
- `mp_memory_arena_remaining()`: Get remaining space
- `mp_memory_arena_used()`: Get used space
- `mp_memory_arena_contains()`: Check if pointer is in arena

---

## Shared Heap Memory

The shared heap provides thread-safe memory allocation with comprehensive tracking and statistics. It's designed for multi-threaded environments where memory needs to be shared between threads.

### Key Features

- Thread-safe allocation with mutex protection
- Allocation tracking with source file and line information
- Memory statistics (used, free, peak, fragmentation)
- Defragmentation support
- Per-thread caching (optional)

### Example Usage

```c
#include "microposix/mm/shared_heap.h"

// Create a shared heap
uint8_t heap_buffer[8192];
mp_shared_heap_config_t config = {
    .buffer = heap_buffer,
    .size = 8192,
    .min_block_size = 16,
    .alignment = 8,
    .thread_safe = true,
    .max_threads = 8
};

mp_shared_heap_t heap;
mp_shared_heap_init(&heap, &config);

// Allocate memory
void *ptr = mp_shared_heap_alloc(&heap, 1024);

// Allocate with tracking
void *ptr2 = mp_shared_heap_alloc_tracked(&heap, 512, __FILE__, __LINE__);

// Free memory
mp_shared_heap_free(&heap, ptr);

// Get statistics
mp_shared_heap_stats_t stats;
mp_shared_heap_get_stats(&heap, &stats);
printf("Used: %zu, Free: %zu, Peak: %zu\n", 
       stats.used_size, stats.free_size, stats.peak_used);

// Check if pointer is in heap
if (mp_shared_heap_contains(&heap, ptr2)) {
    // ptr2 is in this heap
}

// Defragment heap
size_t coalesced = mp_shared_heap_defrag(&heap);

// Reset heap (free all)
mp_shared_heap_reset(&heap);
```

### Global Shared Heap

A global shared heap is available for convenience:

```c
// Initialize global heap
mp_shared_heap_global_init(global_buffer, 16384);

// Allocate from global heap
void *ptr = mp_shared_heap_global_alloc(1024);

// Free from global heap
mp_shared_heap_global_free(ptr);

// With tracking
void *ptr2 = mp_shared_heap_global_alloc_tracked(512);
mp_shared_heap_global_free_tracked(ptr2);
```

### Statistics

The shared heap provides comprehensive statistics:

- `total_size`: Total heap size
- `used_size`: Currently used bytes
- `free_size`: Currently free bytes
- `peak_used`: Peak used bytes
- `num_allocations`: Number of allocations
- `num_frees`: Number of free operations
- `largest_free`: Size of largest free block
- `fragmentation`: Fragmentation percentage

---

## Garbage Collection

microPOSIX provides multiple garbage collection strategies for automatic memory management:

### GC Strategies

1. **Reference Counting**: Simple, deterministic, O(1) overhead
2. **Mark-and-Sweep**: Handles cycles, periodic collection
3. **Generational GC**: Optimized for embedded, young/old generation split

### Example Usage

```c
#include "microposix/mm/gc.h"

// Initialize GC with a shared heap
mp_shared_heap_t heap;
// ... initialize heap ...

mp_gc_config_t gc_config = {
    .heap = &heap,
    .initial_size = 4096,
    .max_objects = 1000,
    .use_ref_counting = true,
    .use_mark_sweep = true,
    .use_generational = true,
    .young_gen_size = 1024,
    .threshold = 100,
    .gc_interval = 1000
};

mp_gc_t gc;
mp_gc_init(&gc, &gc_config);

// Allocate GC-managed object
void *obj = mp_gc_alloc(&gc, 100);

// Allocate with finalizer
void my_finalizer(void *data) {
    // Cleanup code
}
void *obj2 = mp_gc_alloc_with_finalizer(&gc, 200, my_finalizer, NULL);

// Reference counting
mp_gc_incref(&gc, obj);  // Increment reference count
mp_gc_decref(&gc, obj);  // Decrement (frees if count reaches 0)

// Add a root (always considered live)
mp_gc_add_root(&gc, obj, MP_GC_ROOT_GLOBAL, "my_root");

// Run garbage collection
size_t collected = mp_gc_collect(&gc);
printf("Collected %zu objects\n", collected);

// Full collection (mark-and-sweep)
collected = mp_gc_collect_full(&gc);

// Young generation collection
collected = mp_gc_collect_young(&gc);

// Get statistics
mp_gc_stats_t stats;
mp_gc_get_stats(&gc, &stats);
printf("Objects: %zu, Collected: %zu\n", stats.total_objects, stats.objects_collected);

// Cleanup
mp_gc_destroy(&gc);
```

### Global GC

A global GC instance is available:

```c
// Initialize global GC
mp_shared_heap_t heap;
// ... initialize heap ...
mp_gc_global_init(&heap, 1000);

// Allocate from global GC
void *obj = mp_gc_global_alloc(100);

// Reference counting (global)
mp_gc_global_incref(obj);
mp_gc_global_decref(obj);

// Run GC
size_t collected = mp_gc_global_collect();

// Add global root
mp_gc_global_add_root(obj, MP_GC_ROOT_GLOBAL, "global_obj");
```

### GC Macros

Convenience macros for common operations:

```c
// Allocate with type
MyType *obj = mp_gc_alloc_type(&gc, MyType);

// Global allocation with type
MyType *obj2 = mp_gc_global_alloc_type(MyType);

// Declare and initialize a root
MP_GC_ROOT(MyType *, my_root);
MP_GC_ROOT_INIT(&gc, my_root, NULL);

// Stack roots
MP_GC_STACK_ROOT(MyType *, stack_obj);
MP_GC_STACK_PUSH(&gc, stack_obj);
// ... use stack_obj ...
MP_GC_STACK_POP(&gc, stack_obj);
```

### GC Features

- **Reference Counting**: Automatic deallocation when count reaches zero
- **Mark-and-Sweep**: Handles cyclic references
- **Generational GC**: Optimized for embedded systems
- **Finalizers**: Run cleanup code when objects are collected
- **Root Tracking**: Objects that are always considered live
- **Pinning**: Prevent objects from being moved
- **Statistics**: Comprehensive GC statistics

---

## Memory Pools and Arenas

### Fixed-Size Pool Allocator

The pool allocator provides O(1) allocation and freeing for fixed-size blocks, ideal for real-time systems.

```c
#include "microposix/mm/pool.h"

uint8_t pool_buffer[1024];
mp_pool_t pool;

// Initialize pool with 16 blocks of 64 bytes each
mp_pool_init(&pool, pool_buffer, 64, 16);

// Allocate a block
void *block = mp_pool_alloc(&pool);

// Free a block
mp_pool_free(&pool, block);
```

### TLSF Allocator

The Two-Level Segregated Fit (TLSF) allocator provides O(1) allocation and freeing for variable-size blocks with low fragmentation.

```c
#include "microposix/mm/tlsf.h"

uint8_t tlsf_buffer[8192];
mp_tlsf_t tlsf;

mp_tlsf_init(&tlsf, tlsf_buffer, 8192);

// Allocate memory
void *ptr = mp_tlsf_alloc(&tlsf, 1024);

// Free memory
mp_tlsf_free(&tlsf, ptr);
```

---

## Serialization Support

microPOSIX provides comprehensive serialization support for data exchange:

### JSON

Lightweight JSON parser and generator with minimal memory overhead.

```c
#include "microposix/serialization/json.h"

// Parse JSON
const char *json = "{\"name\":\"test\",\"value\":42}";
mp_json_parser_t parser;
mp_json_parser_init(&parser, json);
mp_json_value_t *value = mp_json_parse(&parser, 0);

// Access fields
mp_json_value_t *name = mp_json_object_get(value, "name");
const char *name_str = mp_json_get_string(name);

// Create JSON
mp_json_value_t *obj = mp_json_object();
mp_json_object_add(obj, "name", mp_json_string("test"));
mp_json_object_add(obj, "value", mp_json_integer(42));

// Convert to string
char *json_str = mp_json_to_string(obj, MP_JSON_GEN_PRETTY);
printf("%s\n", json_str);
free(json_str);

// Cleanup
mp_json_free(value);
mp_json_parser_destroy(&parser);
mp_json_free(obj);
```

### Protocol Buffers

Lightweight protobuf implementation for embedded systems.

```c
#include "microposix/serialization/protobuf.h"

// Create a writer
mp_protobuf_writer_t writer;
uint8_t buffer[256];
mp_protobuf_writer_init(&writer, buffer, 256);

// Write a field
mp_protobuf_write_field_header(&writer, 1, MP_PROTOBUF_VARINT);
mp_protobuf_write_varint(&writer, 12345);

// Write a string
mp_protobuf_write_field_header(&writer, 2, MP_PROTOBUF_LENGTH_DELIMITED);
mp_protobuf_write_string(&writer, "Hello");

// Get written data
size_t length = mp_protobuf_writer_get_length(&writer);

// Read data
mp_protobuf_reader_t reader;
mp_protobuf_reader_init(&reader, buffer, length);

uint32_t field_number;
mp_protobuf_wire_type_t wire_type;
mp_protobuf_read_field_header(&reader, &field_number, &wire_type);

uint64_t value;
mp_protobuf_read_varint(&reader, &value);

// Cleanup
mp_protobuf_writer_destroy(&writer);
mp_protobuf_reader_destroy(&reader);
```

### EDF (Extensible Data Format)

Binary serialization format optimized for embedded systems.

```c
#include "microposix/serialization/edf.h"

// Create a writer
mp_edf_writer_t writer;
uint8_t buffer[256];
mp_edf_writer_init(&writer, buffer, 256);

// Write values
mp_edf_write_int32(&writer, 12345);
mp_edf_write_string(&writer, "Hello, EDF!");
mp_edf_write_bool(&writer, true);

// Get written data
size_t length = mp_edf_writer_get_length(&writer);

// Read data
mp_edf_reader_t reader;
mp_edf_reader_init(&reader, buffer, length);

int32_t value;
mp_edf_read_int32(&reader, &value);

char *str;
size_t str_len;
mp_edf_read_string(&reader, &str, &str_len);

bool bool_val;
mp_edf_read_bool(&reader, &bool_val);

// Cleanup
free(str);
mp_edf_writer_destroy(&writer);
mp_edf_reader_destroy(&reader);
```

### Unified Serialization API

```c
#include "microposix/serialization.h"

// Serialize data
typedef struct {
    int id;
    char name[50];
} MyData;

MyData data = {1, "test"};

mp_serialization_options_t options = {
    .format = MP_SERIALIZATION_JSON,
    .pretty_print = true,
    .indent = 2
};

uint8_t buffer[256];
ssize_t written = mp_serialize(MP_SERIALIZATION_JSON, &data, sizeof(data),
                              buffer, 256, &options);

// Deserialize data
MyData decoded;
ssize_t read = mp_deserialize(MP_SERIALIZATION_JSON, buffer, written,
                              &decoded, sizeof(decoded), NULL);
```

---

## Usage Examples

### Example 1: Zero-Copy Protocol Parsing

```c
#include "microposix/mm/memory_view.h"

void parse_protocol_message(uint8_t *buffer, size_t length) {
    mp_memory_view_t view = mp_memory_view_from_buffer(buffer, length);
    
    // Parse header (first 4 bytes)
    mp_memory_view_t header = mp_memory_view_slice(&view, 0, 4);
    uint32_t message_type = *(uint32_t *)header.data;
    
    // Parse payload (remaining bytes)
    mp_memory_view_t payload = mp_memory_view_slice(&view, 4, length - 4);
    
    // Process based on message type
    switch (message_type) {
        case MSG_TYPE_DATA:
            process_data(payload.data, payload.length);
            break;
        case MSG_TYPE_COMMAND:
            process_command(payload.data, payload.length);
            break;
    }
}
```

### Example 2: Thread-Safe Memory Management

```c
#include "microposix/mm/shared_heap.h"
#include "microposix/kernel/thread.h"

mp_shared_heap_t global_heap;

void *thread_function(void *arg) {
    // Allocate memory in thread
    void *data = mp_shared_heap_alloc(&global_heap, 1024);
    
    // Process data...
    
    // Free memory
    mp_shared_heap_free(&global_heap, data);
    
    return NULL;
}

int main(void) {
    uint8_t heap_buffer[16384];
    mp_shared_heap_config_t config = {
        .buffer = heap_buffer,
        .size = 16384,
        .thread_safe = true,
        .max_threads = 8
    };
    
    mp_shared_heap_init(&global_heap, &config);
    
    // Create threads
    mp_thread_t threads[4];
    for (int i = 0; i < 4; i++) {
        mp_thread_create(&threads[i], thread_function, NULL);
    }
    
    // Join threads
    for (int i = 0; i < 4; i++) {
        mp_thread_join(&threads[i], NULL);
    }
    
    // Get heap statistics
    mp_shared_heap_stats_t stats;
    mp_shared_heap_get_stats(&global_heap, &stats);
    printf("Heap usage: %zu/%zu bytes\n", stats.used_size, stats.total_size);
    
    return 0;
}
```

### Example 3: Garbage Collection with Finalizers

```c
#include "microposix/mm/gc.h"

typedef struct {
    int id;
    FILE *file;  // Resource that needs cleanup
} Resource;

void resource_finalizer(void *data) {
    Resource *res = (Resource *)data;
    if (res->file) {
        fclose(res->file);
    }
    printf("Resource %d finalized\n", res->id);
}

int main(void) {
    mp_shared_heap_t heap;
    // ... initialize heap ...
    
    mp_gc_t gc;
    mp_gc_config_t config = {
        .heap = &heap,
        .max_objects = 1000,
        .use_ref_counting = true
    };
    mp_gc_init(&gc, &config);
    
    // Create a resource with finalizer
    Resource *res = (Resource *)mp_gc_alloc_with_finalizer(
        &gc, sizeof(Resource), resource_finalizer, NULL);
    res->id = 1;
    res->file = fopen("data.txt", "r");
    
    // Use the resource...
    
    // When we're done, just let it go out of scope
    // The finalizer will be called when GC collects it
    
    // Force GC to run
    mp_gc_collect(&gc);
    
    mp_gc_destroy(&gc);
    return 0;
}
```

### Example 4: JSON Configuration

```c
#include "microposix/serialization/json.h"

typedef struct {
    const char *name;
    int value;
    bool enabled;
} Config;

Config parse_config(const char *json_str) {
    Config config = {0};
    
    mp_json_parser_t parser;
    mp_json_parser_init(&parser, json_str);
    mp_json_value_t *root = mp_json_parse(&parser, 0);
    
    if (root && mp_json_is_object(root)) {
        mp_json_value_t *name = mp_json_object_get(root, "name");
        if (name && mp_json_is_string(name)) {
            config.name = strdup(mp_json_get_string(name));
        }
        
        mp_json_value_t *value = mp_json_object_get(root, "value");
        if (value && mp_json_is_number(value)) {
            config.value = mp_json_get_integer(value);
        }
        
        mp_json_value_t *enabled = mp_json_object_get(root, "enabled");
        if (enabled && mp_json_is_boolean(enabled)) {
            config.enabled = mp_json_get_boolean(enabled);
        }
    }
    
    mp_json_free(root);
    mp_json_parser_destroy(&parser);
    
    return config;
}

const char *generate_config_json(const Config *config) {
    mp_json_value_t *obj = mp_json_object();
    mp_json_object_add(obj, "name", mp_json_string(config->name));
    mp_json_object_add(obj, "value", mp_json_integer(config->value));
    mp_json_object_add(obj, "enabled", mp_json_boolean(config->enabled));
    
    char *json = mp_json_to_string(obj, MP_JSON_GEN_PRETTY);
    mp_json_free(obj);
    return json;
}
```

---

## Performance Considerations

### Memory Views

- **Pros**: Zero-copy, very fast, no allocation overhead
- **Cons**: Lifetime management can be complex
- **Best for**: Protocol parsing, buffer management, data processing

### Shared Heap

- **Pros**: Thread-safe, comprehensive tracking, defragmentation
- **Cons**: Slightly higher overhead than pool allocator
- **Best for**: General-purpose allocation in multi-threaded environments

### Garbage Collection

- **Reference Counting**: 
  - Pros: Deterministic, O(1) overhead
  - Cons: Can't handle cycles
  - Best for: Simple object graphs without cycles

- **Mark-and-Sweep**:
  - Pros: Handles cycles
  - Cons: Non-deterministic, pause times
  - Best for: Complex object graphs with cycles

- **Generational GC**:
  - Pros: Optimized for embedded, good for short-lived objects
  - Cons: More complex implementation
  - Best for: Systems with many short-lived objects

### Pool Allocator

- **Pros**: O(1) allocation/free, no fragmentation
- **Cons**: Fixed block size, can't allocate larger blocks
- **Best for**: Real-time systems, fixed-size objects

### TLSF Allocator

- **Pros**: O(1) allocation/free, low fragmentation, variable block sizes
- **Cons**: Slightly higher memory overhead
- **Best for**: General-purpose allocation with variable sizes

### Serialization

- **JSON**: Human-readable, higher overhead, good for configuration
- **Protocol Buffers**: Compact binary, fast, good for inter-process communication
- **EDF**: Compact binary, optimized for embedded, good for data storage

---

## API Reference

### Memory Views

See `include/microposix/mm/memory_view.h` for complete API.

### Shared Heap

See `include/microposix/mm/shared_heap.h` for complete API.

### Garbage Collection

See `include/microposix/mm/gc.h` for complete API.

### Memory Pools

See `include/microposix/mm/pool.h` for complete API.

### TLSF Allocator

See `include/microposix/mm/tlsf.h` for complete API.

### JSON

See `include/microposix/serialization/json.h` for complete API.

### Protocol Buffers

See `include/microposix/serialization/protobuf.h` for complete API.

### EDF

See `include/microposix/serialization/edf.h` for complete API.

---

## Building and Testing

To build the memory management and serialization features:

```bash
cd /path/to/microposix
make clean
make
```

To run the tests:

```bash
cd tests
make clean
make
```

This will compile and run all tests, including the new memory and serialization tests.

---

## Configuration

The memory management features can be configured using compiler flags:

```bash
# Enable all memory features
CFLAGS += -DMICROPOSIX_MEMORY_VIEW_ENABLE=1 \
          -DMICROPOSIX_SHARED_HEAP_ENABLE=1 \
          -DMICROPOSIX_GC_ENABLE=1 \
          -DMICROPOSIX_JSON_ENABLE=1 \
          -DMICROPOSIX_PROTOBUF_ENABLE=1 \
          -DMICROPOSIX_EDF_ENABLE=1
```

These flags are automatically set in the main Makefile.

---

## Memory Layout

When using multiple memory management features, consider the memory layout:

```
+------------------+
|   Stack         |  (Thread stacks)
+------------------+
|   Global Heap   |  (Shared heap for general allocation)
+------------------+
|   GC Heap       |  (Heap for GC-managed objects)
+------------------+
|   Pools         |  (Fixed-size pools)
+------------------+
|   Arenas        |  (Temporary arenas)
+------------------+
|   Reserved      |  (For future use)
+------------------+
```

For embedded systems, you might want to place different heaps in different memory regions using linker scripts.

---

## Best Practices

1. **Use memory views for zero-copy operations** when you need to reference portions of buffers without copying.

2. **Use shared heap for general allocation** in multi-threaded environments.

3. **Use pool allocator for real-time systems** where deterministic behavior is critical.

4. **Use GC for complex object graphs** where manual memory management is error-prone.

5. **Use JSON for configuration** and human-readable data.

6. **Use Protocol Buffers or EDF for binary data** exchange between systems.

7. **Always check return values** from allocation functions.

8. **Use tracking features** to identify memory leaks.

9. **Monitor memory usage** using the statistics APIs.

10. **Consider memory layout** when designing your application.

---

## Troubleshooting

### Memory Leaks

Use the tracking features to identify leaks:

```c
// Enable tracking
mp_shared_heap_enable_tracking(&heap, 1000);

// Dump heap information
mp_shared_heap_dump(&heap, my_callback, NULL);
```

### Fragmentation

If you're experiencing fragmentation:

```c
// Defragment the heap
size_t coalesced = mp_shared_heap_defrag(&heap);

// Or use a pool allocator for fixed-size allocations
```

### Out of Memory

Check memory usage:

```c
mp_shared_heap_stats_t stats;
mp_shared_heap_get_stats(&heap, &stats);
printf("Used: %zu, Free: %zu\n", stats.used_size, stats.free_size);
```

Consider:
- Increasing heap size
- Using a different allocator (pool for fixed sizes)
- Running GC more frequently
- Reducing memory usage

---

## Version History

- **v1.0**: Initial release with memory views, shared heap, GC, pools, TLSF
- **v1.1**: Added serialization support (JSON, Protocol Buffers, EDF)
- **v1.2**: Added memory arenas, improved tracking

---

## License

This documentation and the associated code are part of microPOSIX and are licensed under the same terms as microPOSIX itself.

---

## Contributing

Contributions to the memory management and serialization features are welcome. Please:

1. Fork the repository
2. Create a feature branch
3. Make your changes
4. Add tests for new functionality
5. Submit a pull request

Please ensure all tests pass before submitting a pull request.
